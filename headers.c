#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "headers.h"

// #define DEBUG_HTTP_HEADER

#define HTTP_HEADER_BUFFERS 1000
#define HTTP_METHOD_LENGTH 10

#define HTTP_SUCCESS            0
#define HTTP_OUT_OF_BUFFERS     1
#define HTTP_OUT_OF_SEGMENTS    2
#define HTTP_INVALID_HEADER     3
#define HTTP_INVALID_REQUEST    4

struct http_header_buffer {
    int length;
    char *buffer;
};

struct http_header_string {
    int length;                     // length of the string
    int buffer;                     // buffer it starts in
    int offset;                     // offset into the starting buffer
};

struct http_header_whitespace {
    int last_space_tmp_length;      // length of the field at the start of the last whitespace (if valid)
    int last_space_start_length;    // length of the field at the start of the last whitespace
    int last_space_end;             // index of the end of the last whitespace encountered in the field
    int last_space_end_buf;         // buffer the end of the last whitespace in the field was encountered in
    int last_space_end_length;      // length of the field at the end of the last whitespace
    int over_whitespace;            // whether we are currently over whitespace
};

static struct {
    int buf;                        // index of the next buffer to use
    int decoding;                   // index of the current header
    int error;                      // any irrecoverable error the parsing encountered
    int has_request;                // whether the request has been parsed yet
    int has_version;                // whether the request has a version
    int skip_whitespace;            // 0 for not skipping,
                                    // 1 for skipping horizontal whitespace,
                                    // 2 for skipping all whitespace
    struct http_header_whitespace   whitespace;
    struct http_header_buffer       buffers[HTTP_HEADER_BUFFERS];
    struct http_header_string       request[3];
    struct http_header_string       headers[HTTP_HEADER_BUFFERS];
} headers;

static inline int skip_whitespace_horiz (int i, int length, char *buffer) {
    for (; i < length; i++) {
        if (buffer[i] != ' ' && buffer[i] != '\t') {
            headers.skip_whitespace = 0;
            break;
        }
    }
    return i;
}

static inline int skip_whitespace (int i, int length, char *buffer) {
    for (; i < length; i++) {
        if (buffer[i] != ' ' && buffer[i] != '\t' && buffer[i] != '\r' && buffer[i] != '\n') {
            headers.skip_whitespace = 0;
            break;
        }
    }
    return i;
}

int headers_consume (int length, char *buffer) {
    if (headers.error) {
        return headers.error;
    }
    if (headers.buf >= HTTP_HEADER_BUFFERS) {
        return (headers.error = HTTP_OUT_OF_BUFFERS);
    }
    int buf = headers.buf++;
    headers.buffers[buf].length = length;
    headers.buffers[buf].buffer = buffer;
    int i = 0;

    decode:
    if (headers.decoding >= HTTP_HEADER_BUFFERS) {
        return (headers.error = HTTP_OUT_OF_SEGMENTS);
    }
    if (headers.skip_whitespace) {
        if (headers.skip_whitespace == 2) {
            i = skip_whitespace (i, length, buffer);
        } else {
            i = skip_whitespace_horiz (i, length, buffer);
        }
        headers.whitespace.over_whitespace = 0;
        if (headers.has_request) {
            headers.headers[headers.decoding].buffer = buf;
            headers.headers[headers.decoding].offset = i;
        } else {
            headers.request[headers.decoding].buffer = buf;
            headers.request[headers.decoding].offset = i;
        }
    }
    
    if (headers.has_request) { // general case
        if (headers.decoding % 2 == 0) {     // key
            for (; i < length; i++) {
                if (buffer[i] == ':') {
                    i += 2;
                    headers.decoding ++;
                    headers.skip_whitespace = 1;
                    goto decode;
                } else if (buffer[i] == '\n') {
                    return (headers.error = HTTP_INVALID_HEADER);
                }
                headers.headers[headers.decoding].length++;
            }
        } else {                        // value
            for (; i < length; i++) {
                if (buffer[i] == ' ' || buffer[i] == '\t') {
                    if (!headers.whitespace.over_whitespace) {
                        headers.whitespace.over_whitespace = 1;
                        headers.whitespace.last_space_tmp_length = headers.headers[headers.decoding].length;
                    }
                } else if (buffer[i] == '\n') {
                    int cr = ((i > 0 && buffer[i - 1] == '\r') || (buf > 0 &&
                               headers.buffers[buf - 1].buffer[headers.buffers[buf - 1].length - 1] == '\r'));
                    headers.headers[headers.decoding].length = (headers.whitespace.over_whitespace ?
                            headers.whitespace.last_space_tmp_length :
                            headers.headers[headers.decoding].length - (cr ? 1 : 0));
                    i++;
                    headers.decoding++;
                    headers.skip_whitespace = 2;
                    goto decode;
                } else if (buffer[i] != '\r' && headers.whitespace.over_whitespace) {
                    headers.whitespace.over_whitespace = 0;
                }
                headers.headers[headers.decoding].length++;
            }
        }
    } else {                   // request (first line of header)
        if (headers.decoding == 0) {    // method
            for (; i < length; i++) {
                if (buffer[i] == ' ') {
                    headers.whitespace.last_space_start_length = -1;
                    i++;
                    headers.decoding++;
                    headers.skip_whitespace = 1;
                    goto decode;
                } else if (buffer[i] == '\r' || buffer[i] == '\n') {
                    return (headers.error = HTTP_INVALID_REQUEST);
                }
                headers.request[0].length++;
            }
        } else {                        // uri & version
            for (; i < length; i++) {
                if (buffer[i] == ' ' || buffer[i] == '\t') {
                    if (!headers.whitespace.over_whitespace) {
                        headers.whitespace.over_whitespace = 1;
                        headers.whitespace.last_space_tmp_length = headers.request[1].length;
                    }
                } else if (buffer[i] == '\n') {
                    int cr = ((i > 0 && buffer[i - 1] == '\r') || (buf > 0 &&
                            headers.buffers[buf - 1].buffer[headers.buffers[buf - 1].length - 1] == '\r'));
                    if (headers.whitespace.last_space_start_length == -1) {
                        headers.has_version = 0;
                        headers.request[1].length = (headers.whitespace.over_whitespace ?
                                headers.whitespace.last_space_tmp_length :
                                headers.request[1].length - (cr ? 1 : 0));
                    } else {
                        headers.has_version = 1;
                        int orig_length = headers.request[1].length;
                        headers.request[1].length = headers.whitespace.last_space_start_length;
                        headers.request[2].buffer = headers.whitespace.last_space_end_buf;
                        headers.request[2].offset = headers.whitespace.last_space_end;
                        headers.request[2].length =
                            (headers.whitespace.over_whitespace ?
                             headers.whitespace.last_space_tmp_length : orig_length - (cr ? 1 : 0)) - 
                             headers.whitespace.last_space_end_length;

#ifdef DEBUG_HTTP_HEADER
                        printf ("whitespace: length=%d tmp=%d start=%d end=%d,%d,%d\n",
                                orig_length,
                                headers.whitespace.last_space_tmp_length,
                                headers.whitespace.last_space_start_length,
                                headers.whitespace.last_space_end,
                                headers.whitespace.last_space_end_buf,
                                headers.whitespace.last_space_end_length);
#endif
                    }
                    i++;
                    headers.decoding = 0;
                    headers.has_request = 1;
                    headers.skip_whitespace = 2;
                    
                    goto decode;
                } else if (buffer[i] != '\r' && headers.whitespace.over_whitespace) {
                    headers.whitespace.over_whitespace = 0;
                    headers.whitespace.last_space_start_length = headers.whitespace.last_space_tmp_length;
                    headers.whitespace.last_space_end = i;
                    headers.whitespace.last_space_end_buf = buf;
                    headers.whitespace.last_space_end_length = headers.request[1].length;
                }
                headers.request[1].length++;
            }
        }
    }
    return 0;
}

void headers_cleanup () {
    int i;
    for (i = 0; i < headers.buf; i++) {
        free (headers.buffers[i].buffer);
        headers.buffers[i].buffer = NULL;
    }
    headers.has_version = 0;
    headers.has_request = 0;
    headers.buf = 0;
    headers.decoding = 0;
    headers.error = 0;
    headers.skip_whitespace = 0;
    bzero (&headers.whitespace, sizeof (struct http_header_whitespace));
}

static inline char *get_field (struct http_header_string field) {
    if (field.length <= 0) {
        fprintf (stderr, "Empty field (length=%d)\n", field.length);
        return NULL;
    }
    char *dst = malloc ((field.length + 1) * sizeof(char));
    char *pos = dst;
    int rem = field.length;
    int buffer = field.buffer;
    int srclen = headers.buffers[buffer].length - field.offset;
    char *src = headers.buffers[buffer].buffer + field.offset;
    while (rem > 0) {
        while (srclen-- && rem--) {
            *pos++ = *(src++);
        }
        buffer++;
        if (buffer > headers.buf) {
            fprintf (stderr, "Overran buffer\n");
            free (dst);
            return NULL;
        }
        srclen = headers.buffers[buffer].length;
        src = headers.buffers[buffer].buffer;
    }
    dst[field.length] = 0;
    return dst;
    
}

char *get_request (int part) {
    if (!headers.has_request || part < 0 || (!headers.has_version && part > 1) || part > 2) {
        return NULL;
    }
    return get_field (headers.request[part]);
}

char *get_header (int header, int part) {
    if (!headers.has_request || header < 0 || header >= headers.decoding || part < 0 || part > 1) {
        return NULL;
    }
    return get_field (headers.headers[header * 2 + part]);
}

char *headers_get_error (int error) {
    switch (error) {
        case HTTP_SUCCESS:
            return "Success";
        case HTTP_OUT_OF_BUFFERS:
            return "The header parser ran out of buffers to contain the receieved data";
        case HTTP_OUT_OF_SEGMENTS:
            return "The header parser ran out of buffers to contain the parsed headers";
        case HTTP_INVALID_HEADER:
            return "An invalid header was encountered (headers must be of form 'key: value')";
        case HTTP_INVALID_REQUEST:
            return "An invalid request was encountered (requests are the first line of an HTTP request, and must be of form 'method uri version')";
        default:
            return "Unknown error";
    }
}

char *headers_get_current_error () {
    return headers_get_error (headers.error);
}

int headers_has_version () {
    return headers.has_version;
}
