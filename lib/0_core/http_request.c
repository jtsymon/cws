#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "http_request.h"

// #define DEBUG_HTTP_HEADER

#define HTTP_HEADER_BUFFERS 1000
#define HTTP_METHOD_LENGTH 10

#define STATE_READY             0
#define STATE_OUT_OF_BUFFERS    1
#define STATE_OUT_OF_SEGMENTS   2
#define STATE_INVALID_HEADER    3
#define STATE_INVALID_REQUEST   4
#define STATE_END_OF_HEADERS    5
#define STATE_COMPACTED         -1

struct http_header_buffer {
    int length;
    char *buffer;
};

// information about a header field
struct http_header_string {
    // length of the string
    int length;
    // buffer it starts in
    int buffer;
    // offset into the starting buffer
    int offset;
};

struct http_header_whitespace {
    // length of the field at the start of the last whitespace (if valid)
    int last_space_tmp_length;
    // length of the field at the start of the last whitespace
    int last_space_start_length;
    // index of the end of the last whitespace encountered in the field
    int last_space_end;
    // buffer the end of the last whitespace in the field was encountered in
    int last_space_end_buf;
    // length of the field at the end of the last whitespace
    int last_space_end_length;
    // whether we are currently over whitespace
    int over_whitespace;
};

struct http_header_compact {
    char *request[3];
    char *headers[HTTP_HEADER_BUFFERS];
};

static struct {
    // index of the next buffer to use
    int buf;
    // index of the current header
    int decoding;
    // current state of the parser
    int state;
    // whether the request has been parsed yet
    int has_request;
    // whether the request has a version
    int has_version;
    // internal state of the field parser:
    //  0 for not between fields
    //  1 for between key/value pair,
    //  2 for between headers
    //  3 for a folded header
    int between_fields;
    // compacted header strings
    struct http_header_compact      compact;
    // state about whitespace in the current field
    struct http_header_whitespace   whitespace;
    // string buffers we have been given as input
    struct http_header_buffer       buffers[HTTP_HEADER_BUFFERS];
    // information about request header
    struct http_header_string       request[3];
    // information about general headers
    struct http_header_string       headers[HTTP_HEADER_BUFFERS];
} headers;

int headers_consume (int length, char *buffer) {
    if (headers.state) {
        return headers.state;
    }
    if (headers.buf >= HTTP_HEADER_BUFFERS) {
        return (headers.state = STATE_OUT_OF_BUFFERS);
    }
    int buf = headers.buf++;
    headers.buffers[buf].length = length;
    headers.buffers[buf].buffer = buffer;
    int i = 0;

    decode:
    if (headers.decoding >= HTTP_HEADER_BUFFERS) {
        return (headers.state = STATE_OUT_OF_SEGMENTS);
    }
    if (headers.between_fields) {
        for (; i < length; i++) {
            if (buffer[i] == '\n') {
                switch (headers.between_fields) {
                    case 2:
                    case 3:
                        return (headers.state = STATE_END_OF_HEADERS);
                    default:
                        return (headers.state = STATE_INVALID_HEADER);
                }
            } else if (buffer[i] == '\r') {
                // ignore
            } else if (buffer[i] == ' ' || buffer[i] == '\t') {
                if (headers.between_fields == 2) {
                    // folded field (continue from previous)
                    headers.between_fields = 3;
                }
            } else {
#ifdef DEBUG_HTTP_HEADER
                printf ("**** %d\n", headers.between_fields);
#endif
                int next_header = (headers.between_fields == 1);
                if (headers.between_fields == 2) {
                    // start a new field
                    next_header = 1;
                    if (!headers.has_request) {
                        headers.has_request = 1;
                        headers.decoding = -1;
                    }
                } else if (headers.between_fields == 3) {
                    // continue a folded field
#ifdef DEBUG_HTTP_HEADER
                    printf ("Folded field\n");
#endif
                    if (headers.has_request) {
                        headers.headers[headers.decoding].length ++;
                    } else {
                        headers.request[headers.decoding].length ++;
                    }
                }
                if (next_header) {
#ifdef DEBUG_HTTP_HEADER
                    printf ("Advancing header\n");
#endif
                    headers.decoding ++;
                    if (headers.has_request) {
                        headers.headers[headers.decoding].buffer = buf;
                        headers.headers[headers.decoding].offset = i;
                    } else {
                        headers.request[headers.decoding].buffer = buf;
                        headers.request[headers.decoding].offset = i;
                    }
                }

                headers.between_fields = 0;
                headers.whitespace.over_whitespace = 0;
                break;
            }
        }
    }
    
    if (headers.has_request) { // general case
        if (headers.decoding % 2 == 0) {     // key
            for (; i < length; i++) {
                if (buffer[i] == ':') {
                    i += 2;
                    headers.between_fields = 1;
                    goto decode;
                } else if (buffer[i] == '\n') {
                    return (headers.state = STATE_INVALID_HEADER);
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
                    headers.between_fields = 2;
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
                    headers.between_fields = 1;
                    goto decode;
                } else if (buffer[i] == '\r' || buffer[i] == '\n') {
                    return (headers.state = STATE_INVALID_REQUEST);
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
                             headers.whitespace.last_space_tmp_length :
                             orig_length - (cr ? 1 : 0)) -
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
                    headers.between_fields = 2;
                    
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
    for (i = 0; i < (headers.has_version ? 3 : 2); i++) {
        free (headers.compact.request[i]);
    }
    for (i = 0; i <= headers.decoding; i++) {
        free (headers.compact.headers[i]);
        headers.compact.headers[i] = NULL;
    }
    headers.has_version = 0;
    headers.has_request = 0;
    headers.buf = 0;
    headers.decoding = 0;
    headers.state = 0;
    headers.between_fields = 0;
    bzero (&headers.whitespace, sizeof (struct http_header_whitespace));
    bzero (&headers.request, sizeof (headers.request));
    bzero (&headers.headers, sizeof (headers.headers));
    bzero (&headers.compact, sizeof (headers.compact));
}

static inline char *get_field (struct http_header_string field) {
    if (headers.state == STATE_COMPACTED) {
        return NULL;
    }
    if (field.length <= 0) {
#ifdef DEBUG_HTTP_HEADER
        fprintf (stderr, "Empty field (length=%d)\n", field.length);
#endif
        return NULL;
    }
    char *dst = malloc ((field.length + 1) * sizeof(char));
    char *pos = dst;
    int rem = field.length;
    int buffer = field.buffer;
    char *src = headers.buffers[buffer].buffer + field.offset;
    const char *end = headers.buffers[buffer].buffer + headers.buffers[buffer].length;
    int whitespace = 0;
    while (rem > 0) {
        while (src < end && rem > 0) {
            if (!whitespace) {
                if (*src == '\r' || *src == '\n') {
                    whitespace = 1;
                    *pos++ = ' ';
                } else {
                    *pos++ = *(src++);
                }
                rem --;
#ifdef DEBUG_HTTP_HEADER
                *pos = 0;
                printf ("[a] dst = '%s'\n", dst);
#endif
            } else {
                while (src < end &&
                        (*src == '\r' || *src == '\n' || *src == ' ' || *src == '\t')) {
                    src++;
                }
                if (src < end && *src != '\r' && *src != '\n' && *src != ' ' && *src != '\t') {
                    whitespace = 0;
                    *pos++ = *(src++);
                    rem --;
                }
#ifdef DEBUG_HTTP_HEADER
                *pos = 0;
                printf ("[b] dst = '%s'\n", dst);
#endif
            }
        }
        buffer++;
        if (buffer > headers.buf) {
            fprintf (stderr, "Overran buffer\n");
            free (dst);
            return NULL;
        }
#ifdef DEBUG_HTTP_HEADER
        printf ("Next buffer\n");
#endif
        src = headers.buffers[buffer].buffer;
        end = src + headers.buffers[buffer].length;
    }
    dst[field.length] = 0;
    return dst;
}

int headers_compact () {
    // only compact after finished recieving
    if (headers.state != STATE_END_OF_HEADERS || !headers.has_request) {
        return 1;
    }
    int i;
    // compact request
    for (i = 0; i < (headers.has_version ? 3 : 2); i++) {
        if (!headers.compact.request[i]) {
            headers.compact.request[i] = get_field (headers.request[i]);
        }
    }
    // compact headers
    for (i = 0; i <= headers.decoding; i++) {
        if (!headers.compact.headers[i]) {
            headers.compact.headers[i] = get_field (headers.headers[i]);
        }
    }
    // cleanup buffers
    for (i = 0; i < headers.buf; i++) {
        free (headers.buffers[i].buffer);
        headers.buffers[i].buffer = NULL;
    }
    headers.state = STATE_COMPACTED;
    return 0;
}

char *get_request (int part) {
    if (!headers.has_request || part < 0 || (!headers.has_version && part > 1) || part > 2) {
        return NULL;
    }
    if (!headers.compact.request[part]) {
        headers.compact.request[part] = get_field (headers.request[part]);
    }
    return headers.compact.request[part];
}

char *get_header (int header, int part) {
    if (!headers.has_request || header < 0 || header >= headers.decoding || part < 0 || part > 1) {
        return NULL;
    }
    int i = header * 2 + part;
    if (!headers.compact.headers[i]) {
        headers.compact.headers[i] = get_field (headers.headers[i]);
    }
    return headers.compact.headers[i];
}

int headers_is_error (int state) {
    switch (state) {
        case STATE_READY:
        case STATE_END_OF_HEADERS:
            return 0;
        default:
            return 1;
    }
}

int headers_get_state () {
    return headers.state;
}

char *headers_explain_state (int state) {
    switch (state) {
        case STATE_READY:
            return "Ready (no error)";
        case STATE_OUT_OF_BUFFERS:
            return "The header parser ran out of buffers to contain the receieved data";
        case STATE_OUT_OF_SEGMENTS:
            return "The header parser ran out of buffers to contain the parsed headers";
        case STATE_INVALID_HEADER:
            return "An invalid header was encountered (headers must be of form 'key: value')";
        case STATE_INVALID_REQUEST:
            return "An invalid request was encountered (requests are the first line of an HTTP request, and must be of form 'method uri version')";
        case STATE_END_OF_HEADERS:
            return "The headers have already finished";
        default:
            return "Unknown error";
    }
}

int headers_has_version () {
    return headers.has_version;
}

int headers_has_request() {
    return headers.has_request;
}
