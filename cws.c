#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include "cws.h"
#include "io.h"
#include "signals.h"
#include "worker.h"
#include "process.h"

static void signal_handler (int);
static int selfpipe;
static struct termios old_term;

static void setterm() {
    struct termios new_term = old_term;
    new_term.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr (STDIN_FILENO, TCSANOW, &new_term);
}

static void cleanup() {
    puts ("exiting...");
    tcsetattr (STDIN_FILENO, TCSANOW, &old_term);
}

int main (int argc, char **argv) {

    int port = 8080;
    int work = 4;

    sighandleall (&signal_handler, SA_RESTART);

    argv0 = *argv;
    setname ("cws[master]");

    int selfpiperead;
    {
        int pipefd[2];
        if (pipe (pipefd) == -1) {
            die("pipe");
        }
        selfpipe = pipefd[1];
        selfpiperead = pipefd[0];
    }

    worker_init (port);
    printf("Starting\n");
    spawn_workers (work);

    tcgetattr (STDIN_FILENO, &old_term);
    atexit (&cleanup);
    setterm();

    int epollfd = epoll();
    epoll_add (epollfd, STDIN_FILENO);
    epoll_add (epollfd, selfpiperead);

    struct epoll_event events[MAX_QUEUE];

    int i, n;
    char cmd;
    while (1) {
        n = epoll_wait (epollfd, events, MAX_QUEUE, -1);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN))) {
                exit(1);
            } else if (STDIN_FILENO == events[i].data.fd) {
                // user input event
                cmd = getchar();
                switch (cmd) {
                    case 'k':
                        printf ("Killing workers\n");
                        finish(0);
                        break;
                    case 'q':
                        printf ("Telling workers to exit\n");
                        finish(1);
                        break;
                    case '+':
                        spawn_workers (worker_count + 1);
                        break;
                    case '-':
                        spawn_workers (worker_count - 1);
                        break;
                }
            } else {
                // pipe event
                if (read (selfpiperead, &cmd, 1) <= 0) {
                    die ("read");
                }
                switch (cmd) {
                    case CMD_KILL:
                        printf ("Killing workers\n");
                        finish(0);
                        break;
                    case CMD_TERM:
                        printf ("Telling workers to exit\n");
                        finish(1);
                        break;
                    case CMD_INCR:
                        spawn_workers (worker_count + 1);
                        break;
                    case CMD_DECR:
                        spawn_workers (worker_count - 1);
                        break;
                    case CMD_CHLD:
                        check_workers();
                        break;
                }
            }
        }
    }
    return 0; 
}

static void signal_handler (int signo) {
    putchar ('S');
    putchar ('I');
    putchar ('G');
    int d;
    for (d = 1; signo / d >= 10; d *= 10);
    for (; d > 0; d /= 10) {
        putchar ('0' + ((signo / d) % 10));
    }
    putchar('\n');
    switch (signo) {
        case SIGTERM:
            putch (selfpipe, CMD_TERM);
            break;
        case SIGINT:
            putch (selfpipe, CMD_KILL);
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

