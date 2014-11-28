#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define die(msg) do { \
    perror(msg); \
    exit(1); \
} while (0)

#define MAX_QUEUE 64

struct worker {
    int id;
    int pid;
    int pipe;
};

struct worker_list {
    const int count;
    struct worker worker[];
};

#define CMD_KILL 'k'
#define CMD_TERM 't'
#define CMD_INCR 'i'
#define CMD_DECR 'd'
#define CMD_CHLD 'c'

static void catch_signals();
static void ignore_signals();
static void signal_handler(int);
static void child_loop(int, int);
static void spawn_workers(int, struct worker_list**);
static void init_sock(int);
static void poke(int, struct worker*);
static void wait_exit(struct worker_list*);
inline static void putch(int, char);

static int sockfd;
static int selfpipe;

int main(int argc, char **argv) {

    int port = 8080;
    int worker_count = 4;

    catch_signals();

    init_sock(port);

    int selfpiperead;
    {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            die("pipe");
        }
        selfpipe = pipefd[1];
        selfpiperead = pipefd[0];
    }


    struct worker_list *workers = NULL;
    spawn_workers(worker_count, &workers);

    int i;
    char cmd;
    while(1) {
        if (read (selfpiperead, &cmd, 1) <= 0) {
            die ("read");
        }
        switch (cmd) {
            case CMD_KILL:
                printf("Killing workers\n");
                for (i = 0; i < workers->count; i++) {
                    kill(workers->worker[i].pid, SIGTERM);
                }
                wait_exit(workers);
                break;
            case CMD_TERM:
                printf("Telling workers to exit\n");
                for (i = 0; i < workers->count; i++) {
                    putch (workers->worker[i].pipe, CMD_TERM);
                }
                wait_exit(workers);
                break;
            case CMD_INCR:
                spawn_workers(workers->count + 1, &workers);
                break;
            case CMD_DECR:
                spawn_workers(workers->count - 1, &workers);
                break;
            case CMD_CHLD:
                poke (workers->count, workers->worker);
                int result;
                do {
                    result = waitpid (-1, NULL, WNOHANG);
                } while (result && result != -1);
                break;

        }
    }
    return 0; 
}

static void child_loop(int id, int pipe) {
    ignore_signals();
    int clientfd;
    socklen_t clilen;
    struct sockaddr_in cli_addr;
    char buffer[256];

    static int epollfd;
    static struct epoll_event events[MAX_QUEUE];
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        die("epoll_create");
    }

    int flags = fcntl(pipe, F_GETFL, 0);
    if (flags == -1) {
        die("fcntl");
    }
    flags |= O_NONBLOCK;
    if (fcntl(pipe, F_SETFL, flags) == -1) {
        die("fcntl");
    }

    events[0].events = EPOLLIN;
    events[0].data.fd = sockfd;
    events[1].events = EPOLLIN;
    events[1].data.fd = pipe;

    if (epoll_ctl (epollfd, EPOLL_CTL_ADD, sockfd, &events[0]) == -1) {
        die("epoll_ctl");
    }
    if (epoll_ctl (epollfd, EPOLL_CTL_ADD, pipe, &events[1]) == -1) {
        die ("epoll_ctl");
    }

    int i, n;
    while (1) {
        n = epoll_wait(epollfd, events, MAX_QUEUE, -1);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN))) {
                exit(1);
            } else if (sockfd == events[i].data.fd) {
                // socket event
                clilen = sizeof(cli_addr);
                clientfd = accept (sockfd, (struct sockaddr *) &cli_addr, &clilen);
                if (clientfd < 0) {
                    continue;
                }
                bzero(buffer,256);
                if (read (clientfd, buffer, 255) < 0) {
                    continue;
                }
                sprintf(buffer, "Handled by worker #%d\n", id);
                if (write (clientfd, buffer, strlen(buffer)) < 0) {
                    continue;
                }
                close (clientfd);
            } else {
                // pipe event
                char cmd;
                read (pipe, &cmd, 1);
                switch (cmd) {
                    case CMD_TERM:
                        exit(0);
                        break;
                }
            }
        }
    }
}

static void spawn(int id, struct worker *worker) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        die("pipe");
    }
    int pid = fork();
    if (pid) {
        // master
        close(pipefd[0]);
        worker->id = id;
        worker->pid = pid;
        worker->pipe = pipefd[1];

        printf("Child %d is %d\n", id, pid);
    } else {
        // worker
        close(pipefd[1]);
        child_loop(id, pipefd[0]);
        exit(0);
    }
}

static void poke(const int n, struct worker *workers) {
    // check workers are alive
    int i;
    for (i = 0; i < n; i++) {
        int result = waitpid(workers[i].pid, NULL, WNOHANG);
        if (result < -1 || result > 0) {
            // make a new worker
            spawn(i, &workers[i]);
        }
    }
}

static void spawn_workers(int count, struct worker_list **workers) {
    if (count <= 0) return;
    int current;

    if (*workers == NULL) {
        current = 0;
    } else {
        current = (*workers)->count;
    }
    int i;
    if (count == current) return;
    struct worker_list *old_list = *workers;
    *workers = malloc(sizeof(int) + count * sizeof(struct worker));
    *((int*)&(*workers)->count) = count;
    if (count > current) {
        // copy the existing workers
        poke(current, old_list->worker);
        for (i = 0; i < current; i++) {
            (*workers)->worker[i] = old_list->worker[i];
        }
        // add the new workers
        for (i = current; i < count; i++) {
            spawn(i, &(*workers)->worker[i]);
        }
    } else if (count < current) {
        // copy the needed workers
        poke(count, old_list->worker);
        for (i = 0; i < count; i++) {
            (*workers)->worker[i] = old_list->worker[i];
        }
        // kill the surplus workers
        for (i = count; i < current; i++) {
            putch (old_list->worker[i].pipe, CMD_TERM);
        }
    }
}

static void wait_exit(struct worker_list *workers) {
    int count = workers->count;
    while (count) {
        if (wait(NULL) != -1) {
            count--;
        }
    }
    exit(0);
}


static void init_sock(int port) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        die("socket");
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        die("bind");
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        die("fcntl");
    }
    flags |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        die("fcntl");
    }

    if (listen(sockfd, SOMAXCONN) == -1) {
        die("listen");
    }
}

inline static void putch (int fd, char ch) {
    write (fd, &ch, 1);
}

static void signal_handler(int signo) {
    static const char *msg = "\nCaught: ";
    write(1, msg, strlen(msg));
    int i;
    for (i = signo; i > 0; i /= 10) {
        putchar('0' + (i % 10));
    }
    putchar('\n');
    switch (signo) {
        case SIGTERM:
            putch (selfpipe, CMD_KILL);
            break;
        case SIGINT:
            putch (selfpipe, CMD_TERM);
            break;
        case SIGUSR1:
            putch (selfpipe, CMD_INCR);
            break;
        case SIGUSR2:
            putch (selfpipe, CMD_DECR);
            break;
        case SIGCHLD:
            putch (selfpipe, CMD_CHLD);
            break;
    }
}

static void catch_signals() {
    struct sigaction act = {
        .sa_handler = &signal_handler,
        .sa_flags = SA_RESTART
    };
    sigfillset(&act.sa_mask);
    if (sigaction(SIGINT,  &act, NULL) ||
        sigaction(SIGQUIT, &act, NULL) ||
        sigaction(SIGTERM, &act, NULL) ||
        sigaction(SIGCHLD, &act, NULL) ||
        sigaction(SIGHUP,  &act, NULL) ||
        sigaction(SIGUSR1, &act, NULL) ||
        sigaction(SIGUSR2, &act, NULL)) {
            printf("%s\n", strerror(errno));
            die("sigaction\n");
    }
}

static void ignore_signals() {
    struct sigaction ign = {
        .sa_handler = SIG_IGN
    };
    struct sigaction dfl = {
        .sa_handler = SIG_DFL
    };
    if (sigaction(SIGINT,  &ign, NULL) ||
        sigaction(SIGQUIT, &ign, NULL) ||
        sigaction(SIGTERM, &dfl, NULL) ||
        sigaction(SIGCHLD, &dfl, NULL) ||
        sigaction(SIGHUP,  &ign, NULL) ||
        sigaction(SIGUSR1, &ign, NULL) ||
        sigaction(SIGUSR2, &ign, NULL)) {
            printf("%s\n", strerror(errno));
            die("sigaction\n");
    }
}
