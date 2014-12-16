#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "http_request.h"
#include "http_response.h"
#include "io.h"

void handle (int worker_id, int sock_fd, struct sockaddr addr, socklen_t addr_len) {
    if (sock_fd < 0) {
        return;
    }
    fd_set set;
    struct timeval timeout;
    FD_ZERO (&set);
    FD_SET (sock_fd, &set);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    headers_cleanup ();
    while (select (FD_SETSIZE, &set, NULL, NULL, &timeout) == 1) {
        char *buffer = malloc(255);
        int len = read (sock_fd, buffer, 255);
        if (len <= 0 || headers_consume (len, buffer)) {
            break;
        }
    }
    if (!headers_has_request()) {
        goto hangup;
    }
    int j;
    response_init (200);
    int headers_len = 0;
    char *headers = get_response(&headers_len);
    if (!headers || headers_len <= 0 || write (sock_fd, headers, headers_len) < 0) {
        goto hangup;
    }
    char response_buffer[255];
    sprintf (response_buffer, "Handled by worker #%d\nRequest:\n", worker_id);
    if (write (sock_fd, response_buffer, strlen (response_buffer)) < 0) {
        goto hangup;
    }

    char *field;
    if ((field = get_request (0))) {
        sprintf (response_buffer, "Method: %s\n", field);
        free (field);
        if (write (sock_fd, response_buffer, strlen (response_buffer)) < 0) {
            goto hangup;
        }
    }
    if ((field = get_request (1))) {
        sprintf (response_buffer, "Path: %s\n", field);
        free (field);
        if (write (sock_fd, response_buffer, strlen (response_buffer)) < 0) {
            goto hangup;
        }
    }
    if (headers_has_version () && (field = get_request (2))) {
        sprintf (response_buffer, "Version: %s\n", field);
        free (field);
        if (write (sock_fd, response_buffer, strlen (response_buffer)) < 0) {
            goto hangup;
        }
    }

    sprintf (response_buffer, "Headers:\n");
    if (write (sock_fd, response_buffer, strlen (response_buffer)) < 0) {
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
        if (write (sock_fd, key, strlen(key)) < 0 ||
                write (sock_fd, " = ", 3) < 0 ||
                write (sock_fd, val, strlen(val)) < 0 ||
                write (sock_fd, "\n", 1) < 0) {
            free (key);
            free (val);
            break;
        }
        free (key);
        free (val);
    }
hangup:
    getSO_ERROR(sock_fd);
    if (close (sock_fd)) {
        fprintf (stderr, "Failed to close socket\n");
    }
}
