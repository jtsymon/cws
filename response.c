#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "response.h"

#define HTTP_VERSION    "HTTP/1.1"
static const char *http_response[] = {
#include "http_responses.txt"
};

struct additional_headers;
struct additional_headers {
    struct additional_headers *next;
    char *key;
    char *val;
};

struct {
    int code;
    struct additional_headers *headers;
    struct additional_headers *last;
} response;

const char *response_message (int code) {
    if (code >= 100 && code < 600) {
        return http_response[code - 100];
    }
    return NULL;
}

void response_init (int code) {
    response.code = code;
    response.last = NULL;
    while (response.headers) {
        struct additional_headers *current = response.headers;
        response.headers = current->next;
        free (current->key);
        free (current->val);
        free (current);
    }
}

void response_add (char *key, char *val) {
    struct additional_headers *header = malloc (sizeof (struct additional_headers));
    header->key = key;
    header->val = val;
    if (response.last) {
        response.last->next = header;
    } else {
        response.headers = header;
    }
    response.last = header;
}

char *get_response () {
    if (response.code < 100 || response.code >= 600) {
        return NULL;
    }
    const char *message = response_message (response.code);
    int length = strlen (HTTP_VERSION) + 1 + 3 + 1 + strlen (message) + 2 + 2;
    struct additional_headers *header;
    header = response.headers;
    while (header) {
        length += strlen (header->key) + 2 + strlen (header->val) + 2;
        header = header->next;
    }
    char *result = malloc (length);
    char *pos = result;
    int wrote = snprintf (pos, length, "%s %d %s\r\n", HTTP_VERSION, response.code, message);
    pos += wrote;
    length -= wrote;
    header = response.headers;
    while (header) {
        wrote = snprintf (pos, length, "%s: %s\r\n", header->key, header->val);
        pos += wrote;
        length -= wrote;
        header = header->next;
    }
    *pos++ = '\r';
    *pos++ = '\n';
    length -= 2;
    return result;
}
