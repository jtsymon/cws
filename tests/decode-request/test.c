#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_request.h"

#define indent "  "

static void print_request (char *key, char *val) {
    if (key && val) {
        printf (indent "%s: %s\n", key, val);
        free (val);
    } else {
        exit (1);
    }
}

static void test_headers (int size, int len, char *str) {
    int parts= len / size;
    int rem  = len % size;
    while (parts--) {
        char *part = malloc (size + 1);
        part[size] = 0;
        memcpy (part, str, size);
        str += size;
        headers_consume (size, part);
    }
    if (rem) {
        char *part = malloc (rem + 1);
        part[rem] = 0;
        memcpy (part, str, rem);
        if (headers_consume (rem, part)) {
            fprintf (stderr, indent "Error: %s\n", headers_get_current_error());
            exit (1);
        }
    }
    if (!headers_has_request()) {
        exit(1);
    }
    print_request ("Method", get_request (0));
    print_request ("Path",   get_request (1));
    if (headers_has_version()) {
        print_request ("Version", get_request (2));
    }
    int i = 0;
    while (1) {
        char *key = get_header (i, 0),
             *val = get_header (i, 1);
        if (key && val) {
            printf (indent "%s: %s\n", key, val);
            free (key);
            free (val);
        } else if (key || val) {
            exit (1);
        } else {
            break;
        }
        i++;
    }
}

static char *requests[] = {
    "GET /index.html HTTP/1.1\r\n\
     Host: example.com\n",

    "GET /index.html\r\n\
     Host: example.com\n",

    "GET /index.html HTTP/1.1\r\n\
     Host: example.com\n\
     Multiline: line1\n\
                line2\r\n",

    NULL
};

int main (void) {
    int i;
    for (i = 0; requests[i]; i++) {
        printf ("Testing request %d:\n", i + 1);
        test_headers (10, strlen (requests[i]), requests[i]);
        headers_cleanup ();
    }
    return 0;
}
