#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#include "misc.h"
#include "io.h"

inline void putch (int fd, char ch) {
    write (fd, &ch, 1);
}

int init_sock (int port) {
    int fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket");
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons (port)
    };
    if (bind (fd, (struct sockaddr *) &serv_addr, sizeof (serv_addr)) < 0) {
        die("bind");
    }

    non_block (fd);

    if (listen (fd, SOMAXCONN) == -1) {
        die("listen");
    }

    return fd;
}

void epoll_add (int epollfd, int fd) {
    struct epoll_event event = {
        .events = EPOLLIN,
        .data.fd = fd
    };
    if (epoll_ctl (epollfd, EPOLL_CTL_ADD, fd, &event) == -1) {
        die("epoll_ctl");
    }
}

int epoll() {
    int epollfd;
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        die("epoll_create");
    }
    return epollfd;
}

void non_block (int fd) {
    int flags = fcntl (fd, F_GETFL, 0);
    if (flags == -1) {
        die("fcntl");
    }
    flags |= O_NONBLOCK;
    if (fcntl (fd, F_SETFL, flags) == -1) {
        die("fcntl");
    }
}

int getSO_ERROR(int fd) {
    int err = 1;
    socklen_t len = sizeof err;
    if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len))
        fprintf (stderr, "getSO_ERROR\n");
    if (err)
        errno = err;              // set errno to the socket SO_ERROR
    return err;
}
