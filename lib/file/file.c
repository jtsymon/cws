#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "http_request.h"
#include "http_response.h"
#include "io.h"

int write_headers (int fd) {
    int headers_len;
    char *headers = get_response(&headers_len);
    int error = (!headers || headers_len <= 0 || write (fd, headers, headers_len) < 0);
    free (headers);
    return error;
}

void _200 (int sock_fd, int file_fd) {
    return;
}

void run (int worker_id, int sock_fd, struct sockaddr addr, socklen_t addr_len) {
    
    headers_parse (sock_fd);
    if (headers_is_error (headers_get_state ()) || !headers_has_request()) {
        goto hangup;
    }
    
    char *name = get_request (1);
    char *strip = name;
    char response_buffer[255];
    while (*strip == '.' || *strip == '/') strip++;
    if (!*strip) {
        fprintf (stderr, "%s: No such file or directory\n", name);
        goto _404;
    }
    int len = strlen (strip);
    int file_fd;
    char *file = malloc (2 + len + 1);
    file[0] = '.';
    file[1] = '/';
    memcpy (file + 2, strip, len);
    file[len + 2] = 0;
    file_fd = open (file, O_RDONLY);
    free (file);

    struct stat info;
    if (file_fd == -1 || fstat (file_fd, &info)) {
        if (file_fd > 0) {
            close (file_fd);
        }
        file_fd = 0;
        perror (name);
        goto _404;
    }

    response_init (200);
    if (write_headers (sock_fd)) {
        close (file_fd);
        goto _404;
    }
    len = read (file_fd, response_buffer, 255);
    while (len > 0) {
        if (write (sock_fd, response_buffer, len) < 0) {
            perror ("write");
            break;
        }
        len = read (file_fd, response_buffer, 255);
    }

    if (file_fd > 0) {
        close (file_fd);
    }
    goto hangup;

_404:
    response_init (404);
    write_headers (sock_fd);
    snprintf (response_buffer, 255, "File not found: '%s'\n", name);
    if (write (sock_fd, response_buffer, strlen (response_buffer)) < 0) {
        goto hangup;
    }
    goto hangup;

hangup:
    headers_cleanup ();
    hangup (sock_fd);
}
