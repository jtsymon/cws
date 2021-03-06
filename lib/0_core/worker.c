#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <errno.h>

#include "misc.h"
#include "worker.h"
#include "signals.h"
#include "io.h"
#include "http_request.h"
#include "http_response.h"
#include "process.h"
#include "plugin.h"

struct worker {
    int id;
    int pid;
    int pipe;
};

static void cleanup ();
static int worker_sock;
const int worker_count;
static struct worker *workers = NULL;
static int worker_id;
static struct plugin_t handler;

static void work (int pipe) {
    setname ("cws[worker]#%d", worker_id);

    sighandleall (SIG_IGN, 0);
    sighandle (SIG_DFL, 0, SIGTERM);

    non_block (pipe);

    int epollfd = epoll();
    epoll_add (epollfd, worker_sock);
    epoll_add (epollfd, pipe);
    struct epoll_event events[MAX_QUEUE];

    int i, n;
    while (1) {
        n = epoll_wait (epollfd, events, MAX_QUEUE, -1);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN))) {
                cleanup ();
                _Exit(1);
            } else if (worker_sock == events[i].data.fd) {
                // socket event
                struct sockaddr cli_addr;
                socklen_t cli_len = sizeof (cli_addr);
                int cli_fd = accept (worker_sock, (struct sockaddr *) &cli_addr, &cli_len);
                handler.function (worker_id, cli_fd, cli_addr, cli_len);
            } else {
                // pipe event
                char cmd;
                read (pipe, &cmd, 1);
                switch (cmd) {
                    case CMD_TERM:
                        cleanup ();
                        _Exit(0);
                        break;
                }
            }
        }
    }
}

void worker_init (int port, const char *handler_plugin) {
    printf ("Starting on port: %d\n", port);
    worker_sock = init_sock (port);
    non_block (worker_sock);
    if (load_plugin (&handler, handler_plugin)) {
        die ("load_plugin");
    }
}

static void respawn (int id) {
    int pipefd[2];
    if (pipe (pipefd) == -1) {
        die("pipe");
    }
    int seed = rand();
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
        srand (seed);
        worker_id = id;
        close (pipefd[1]);
        work (pipefd[0]);
        _Exit(0);
    }
}

void check_workers() {
    // check workers are alive
    int i;
    // acknowledge dead workers
    while (waitpid (-1, NULL, WNOHANG) > 0);
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
    cleanup ();
    exit(0);
}

static void cleanup () {
    free_plugin (&handler);
    free (workers);
}
