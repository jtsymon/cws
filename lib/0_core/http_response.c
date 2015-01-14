#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "http_response.h"

#define HTTP_VERSION    "HTTP/1.1"
#define HTTP_HEADER_BUFFERS 1000
static const char *http_response[] = {
#include "http_responses.txt"
};

struct http_header {
    char *key;
    char *val;
};

static struct {
    int code;
    int header_count;
    struct http_header headers[HTTP_HEADER_BUFFERS];
} response;

const char *response_message (int code) {
    if (code >= 100 && code < 600) {
        return http_response[code - 100];
    }
    return NULL;
}

void response_init (int code) {
    response.code = code;
    response.header_count = 0;
}

void response_add (char *key, char *val) {
    struct http_header *header = &response.headers[response.header_count ++];
    header->key = key;
    header->val = val;
}

char *get_response (int *len) {
    if (response.code < 100 || response.code >= 600) {
        return NULL;
    }
    const char *message = response_message (response.code);
    int length = strlen (HTTP_VERSION) + 1 + 3 + 1 + strlen (message) + 2 + 2;
    int i;
    for (i = 0; i < response.header_count; i++) {
        struct http_header *header = &response.headers[i];
        length += strlen (header->key) + 2 + strlen (header->val) + 2;
    }
    char *result = malloc (length);
    char *pos = result;
    int wrote = snprintf (pos, length, "%s %d %s\r\n", HTTP_VERSION, response.code, message);
    pos += wrote;
    length -= wrote;
    for (i = 0; i < response.header_count; i++) {
        struct http_header *header = &response.headers[i];
        wrote = snprintf (pos, length, "%s: %s\r\n", header->key, header->val);
        pos += wrote;
        length -= wrote;
    }
    *pos++ = '\r';
    *pos++ = '\n';
    length -= 2;
    if (len) {
        *len = (pos - result);
    }
    return result;
}
