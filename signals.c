#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "cws.h"
#include "signals.h"

void sighandle (void (*handler)(int), int flags, int sig) {
    struct sigaction act = {
        .sa_handler = handler,
        .sa_flags = flags
    };
    sigfillset(&act.sa_mask);
    if (sigaction (sig,  &act, NULL)) {
        die ("sigaction");
    }
}

void sighandleall (void (*handler)(int), int flags) {
    struct sigaction act = {
        .sa_handler = handler,
        .sa_flags = flags
    };
    sigfillset(&act.sa_mask);
    if (sigaction (SIGINT,  &act, NULL) ||
        sigaction (SIGQUIT, &act, NULL) ||
        sigaction (SIGTERM, &act, NULL) ||
        sigaction (SIGCHLD, &act, NULL) ||
        sigaction (SIGHUP,  &act, NULL) ||
        sigaction (SIGUSR1, &act, NULL) ||
        sigaction (SIGUSR2, &act, NULL)) {
            die("sigaction");
    }
}
