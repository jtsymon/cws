#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "headers.h"

#define HTTP_HEADER_BUFFERS 1000

struct http_header_buffer {
    int length;
    char *buffer;
};

struct http_header_string {
    int length;                     // length of the string
    int index;                      // buffer it starts in
    int offset;                     // offset into the starting buffer
};

static struct {
    int buf;                        // index of the next buffer to use
    int carriage_return;            // whether the previous character was \r
    int decoding;                   // index of the current header
    struct http_header_buffer buffers[HTTP_HEADER_BUFFERS];
    struct http_header_string headers[HTTP_HEADER_BUFFERS];
} headers;

void headers_consume (int length, char *buffer) {
    if (headers.decoding >= HTTP_HEADER_BUFFERS || headers.buf >= HTTP_HEADER_BUFFERS) {
        fprintf (stderr, "OUT OF ROOM\n");
        return;
    }
    headers.buffers[headers.buf].length = length;
    headers.buffers[headers.buf].buffer = buffer;
    char *p = buffer;
    while (length--) {
        headers.headers[headers.decoding].length++;
        if (headers.carriage_return) {
            headers.carriage_return = 0;
            if (*p == '\n') {
                // end of header
                headers.headers[headers.decoding].length -= 2;
                headers.decoding++;
                if (headers.decoding >= HTTP_HEADER_BUFFERS) {
                    fprintf (stderr, "OUT OF ROOM\n");
                    return;
                }
                if (length) {
                    headers.headers[headers.decoding].index = headers.buf;
                    headers.headers[headers.decoding].offset = (p - buffer) + 1;
                } else {
                    headers.headers[headers.decoding].index = headers.buf + 1;
                    headers.headers[headers.decoding].offset = 0;
                }
                continue;
            }
        }
        if (*p == '\r') headers.carriage_return = 1;
        p++;
    }
    headers.buf++;
}

void headers_cleanup () {
    int i;
    for (i = 0; i < headers.buf; i++) {
        free (headers.buffers[i].buffer);
        headers.buffers[i].buffer = NULL;
    }
    headers.buf = 0;
    headers.carriage_return = 0;
    headers.decoding = 0;
}

char *get (int id) {
    struct http_header_string field = headers.headers[id];
    if (field.length <= 0) {
        fprintf (stderr, "Empty field (length=%d)\n", field.length);
        return NULL;
    }
    char *dst = malloc ((field.length + 1) * sizeof(char));
    char *pos = dst;
    int rem = field.length;
    int index = field.index;
    int offset = field.offset;
    int srclen = headers.buffers[index].length - offset;
    char *src = headers.buffers[index].buffer + offset;
    while (rem > 0) {
        while (srclen-- && rem--) {
            *pos++ = *(src++);
        }
        index++;
        if (index > headers.buf) {
            fprintf (stderr, "Overran buffer\n");
            free (dst);
            return NULL;
        }
        srclen = headers.buffers[index].length;
        src = headers.buffers[index].buffer;
    }
    dst[field.length] = 0;
    return dst;
}
