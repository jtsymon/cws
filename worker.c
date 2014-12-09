#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "cws.h"
#include "worker.h"
#include "signals.h"
#include "io.h"
#include "headers.h"
#include "response.h"

struct worker {
    int id;
    int pid;
    int pipe;
};

static int worker_sock;
const int worker_count;
static struct worker *workers = NULL;

static void work (int id, int pipe) {
    sighandleall (SIG_IGN, 0);
    sighandle (SIG_DFL, 0, SIGTERM);
    int clientfd;
    socklen_t clilen;
    struct sockaddr_in cli_addr;
    char *buffer;
    char response_buffer[255];

    non_block (pipe);

    int epollfd = epoll();
    epoll_add (epollfd, worker_sock);
    epoll_add (epollfd, pipe);
    struct epoll_event events[MAX_QUEUE];

    int i, n;
    while (1) {
        headers_cleanup ();
        n = epoll_wait (epollfd, events, MAX_QUEUE, -1);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN))) {
                _Exit(1);
            } else if (worker_sock == events[i].data.fd) {
                // socket event
                clilen = sizeof (cli_addr);
                clientfd = accept (worker_sock, (struct sockaddr *) &cli_addr, &clilen);
                if (clientfd < 0) {
                    continue;
                }
                int len;
                do {
                    buffer = malloc (255);
                    len = read (clientfd, buffer, 255);
                    if (headers_consume (len, buffer)) {
                        break;
                    }
                } while (len > 0);
                
                int j;
                response_init (200);
                sprintf (response_buffer, "%sHandled by worker #%d\nRequest:\n", get_response(), id);
                if (write (clientfd, response_buffer, strlen (response_buffer)) < 0) {
                    goto hangup;
                }
                char *field;
                if ((field = get_request (0))) {
                    sprintf (response_buffer, "Method: %s\n", field);
                    free (field);
                    if (write (clientfd, response_buffer, strlen (response_buffer)) < 0) {
                        goto hangup;
                    }
                }
                if ((field = get_request (1))) {
                    sprintf (response_buffer, "Path: %s\n", field);
                    free (field);
                    if (write (clientfd, response_buffer, strlen (response_buffer)) < 0) {
                        goto hangup;
                    }
                }
                if (headers_has_version () && (field = get_request (2))) {
                    if (field) {
                        sprintf (response_buffer, "Version: %s\n", field);
                        free (field);
                        if (write (clientfd, response_buffer, strlen (response_buffer)) < 0) {
                            goto hangup;
                        }
                    }
                }

                sprintf (response_buffer, "Headers:\n");
                if (write (clientfd, response_buffer, strlen (response_buffer)) < 0) {
                    goto hangup;
                }
                for (j = 0; ; j++) {
                    char *key = get_header (j, 0);
                    char *val = get_header (j, 1);
                    if (!key || !val) {
                        free (key);
                        free (val);
                        break;
                    }
                    if (write (clientfd, key, strlen(key)) < 0 ||
                            write (clientfd, " = ", 3) < 0 ||
                            write (clientfd, val, strlen(val)) < 0 ||
                            write (clientfd, "\n", 1) < 0) {
                        free (key);
                        free (val);
                        goto hangup;
                    }
                    free (key);
                    free (val);
                }
                hangup:
                close (clientfd);
            } else {
                // pipe event
                char cmd;
                read (pipe, &cmd, 1);
                switch (cmd) {
                    case CMD_TERM:
                        _Exit(0);
                        break;
                }
            }
        }
    }
}

void worker_init (int port) {
    printf ("Starting on port: %d\n", port);
    worker_sock = init_sock (port);
    non_block (worker_sock);
}

static void respawn (int id) {
    int pipefd[2];
    if (pipe (pipefd) == -1) {
        die("pipe");
    }
    int pid = fork();
    if (pid) {
        // master
        close (pipefd[0]);
        workers[id].id = id;
        workers[id].pid = pid;
        workers[id].pipe = pipefd[1];

        printf("Child %d is %d\n", id, pid);
    } else {
        // worker
        close (pipefd[1]);
        work (id, pipefd[0]);
        _Exit(0);
    }
}

void check_workers() {
    // check workers are alive
    int i;
    for (i = 0; i < worker_count; i++) {
        int result = waitpid (workers[i].pid, NULL, WNOHANG);
        if (result < -1 || result > 0) {
            // make a new worker
            respawn (i);
        }
    }
}

void spawn_workers (int new_count) {
    if (new_count <= 0 || new_count == worker_count) return;

    int old_count;
    if (workers == NULL) {
        old_count = 0;
    } else {
        old_count = worker_count;
    }
    *((int*)&worker_count) = new_count;
    printf ("Workers: %d\n", worker_count);
    struct worker *old_list = workers;
    workers = malloc (new_count * sizeof (struct worker));

    int i;
    if (new_count > old_count) {
        // copy the existing workers
        for (i = 0; i < old_count; i++) {
            memcpy (&workers[i], &old_list[i], sizeof (struct worker));
        }
        // add the new workers
        for (i = old_count; i < new_count; i++) {
            respawn (i);
        }
    } else if (new_count < old_count) {
        // copy the needed workers
        for (i = 0; i < new_count; i++) {
            memcpy (&workers[i], &old_list[i], sizeof (struct worker));
        }
        // kill the surplus workers
        sigset_t new_mask, old_mask;
        sigaddset (&new_mask, SIGCHLD);
        sigprocmask (SIG_BLOCK, &new_mask, &old_mask);
        for (i = new_count; i < old_count; i++) {
            putch (old_list[i].pipe, CMD_TERM);
        }
        sigprocmask (SIG_SETMASK, &old_mask, NULL);
    }
}

void finish (int force) {
    int i;
    if (force) {
        for (i = 0; i < worker_count; i++) {
            kill (workers[i].pid, SIGTERM);
        }
    } else {
        for (i = 0; i < worker_count; i++) {
            putch (workers[i].pipe, CMD_TERM);
        }
    }
    int remaining = worker_count;
    while (remaining) {
        if (wait (NULL) != -1) {
            remaining--;
        }
    }
    exit(0);
}

