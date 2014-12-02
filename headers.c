#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "headers.h"

#define HTTP_HEADER_BUFFERS 1000
#define HTTP_METHOD_LENGTH 10

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
    int decoding;                   // index of the current header
    struct http_header_buffer buffers[HTTP_HEADER_BUFFERS];
    struct http_header_string headers[HTTP_HEADER_BUFFERS];
} headers;

void headers_consume (int length, char *buffer) {
    if (headers.decoding >= HTTP_HEADER_BUFFERS || headers.buf >= HTTP_HEADER_BUFFERS) {
        fprintf (stderr, "OUT OF ROOM\n");
        return;
    }
    int buf = headers.buf++;
    headers.buffers[buf].length = length;
    headers.buffers[buf].buffer = buffer;
    int i;
    for (i = 0; i < length; i++) {
        headers.headers[headers.decoding].length++;
        if (buffer[i] == '\n' && (
                (i > 0 && buffer[i - 1] == '\r') || (
                    buf > 0 &&
                    headers.buffers[buf - 1].buffer[headers.buffers[buf - 1].length - 1] == '\r'))) {
            // end of header
            headers.headers[headers.decoding].length -= 2;
            headers.decoding++;
            if (headers.decoding >= HTTP_HEADER_BUFFERS) {
                fprintf (stderr, "OUT OF ROOM\n");
                return;
            }
            if (length) {
                headers.headers[headers.decoding].index = buf;
                headers.headers[headers.decoding].offset = i + 1;
            } else {
                headers.headers[headers.decoding].index = buf + 1;
                headers.headers[headers.decoding].offset = 0;
            }
        }
    }
}

void headers_cleanup () {
    int i;
    for (i = 0; i < headers.buf; i++) {
        free (headers.buffers[i].buffer);
        headers.buffers[i].buffer = NULL;
    }
    headers.buf = 0;
    headers.decoding = 0;
}

char *get_header (int id) {
    struct http_header_string field = headers.headers[id];
    if (field.length <= 0) {
        fprintf (stderr, "Empty field (length=%d)\n", field.length);
        return NULL;
    }
    char *dst = malloc ((field.length + 1) * sizeof(char));
    char *pos = dst;
    int rem = field.length;
    int index = field.index;
    int srclen = headers.buffers[index].length - field.offset;
    char *src = headers.buffers[index].buffer + field.offset;
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

char *get_method () {
    struct http_header_string field = headers.headers[0];
    if (field.length <= 0) {
        fprintf (stderr, "Empty field (length=%d)\n", field.length);
        return NULL;
    }
    char *dst = malloc (HTTP_METHOD_LENGTH * sizeof(char));
    char *pos = dst;
    int rem = field.length;
    int index = field.index;
    int srclen = headers.buffers[index].length - field.offset;
    char *src = headers.buffers[index].buffer + field.offset;
    while (rem > 0) {
        while (srclen-- && rem-- && *src != ' ') {
            *pos++ = *(src++);
        }
        if (*src == ' ') {
            *pos++ = 0;
            return dst;
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
    free (dst);
    return NULL;
}
