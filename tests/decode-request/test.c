#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_request.h"

#define indent "  "

void fail () {
    headers_cleanup ();
    exit (1);
}

static int print_header (char *key, char *val) {
    if (key && val) {
        printf (indent "%s: %s\n", key, val);
    } else if (key || val) {
        fprintf (stderr, indent "Header contained null field\n");
        printf (indent "%s: %s\n", key, val);
        fail ();
    } else {
        return 0;
    }
    return 1;
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
        if (headers_is_error(headers_consume (rem, part))) {
            fprintf (stderr, indent "Error: %s\n", headers_explain_state(headers_get_state()));
            fail ();
        }
    }
    headers_compact();
    if (!headers_has_request()) {
        fprintf (stderr, indent "Failed to parse request line\n");
        fail ();
    }
    print_header ("Method", get_request (0));
    print_header ("Path",   get_request (1));
    if (headers_has_version()) {
        print_header ("Version", get_request (2));
    }
    int i = 0;
    while (print_header (get_header (i, 0), get_header (i, 1))) {
        i++;
    }
    headers_cleanup ();
}

static char *requests[] = {
"GET /index.html HTTP/1.1\r\n\
Host: example.com\n\
\r\n",

"GET /index.html\r\n\
Host: example.com\n\
 \r\n",

"GET /index.html HTTP/1.1\r\n\
Host: example.com\n\
Multiline: line1\n\
 line2\r\n\
    \t   \t    line3\n\
           test:    zyxw\n\
Last: ------\n\
 \t \r\n",

NULL
};

int main (void) {
    int i;
    for (i = 0; requests[i]; i++) {
        printf ("Testing request %d:\n", i + 1);
        test_headers (10, strlen (requests[i]), requests[i]);
    }
    return 0;
}
