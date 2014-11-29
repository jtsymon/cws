#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define die(msg) do { \
    perror(msg); \
    exit(1); \
} while (0)

#define MAX_QUEUE 64

#define CMD_KILL 'k'
#define CMD_TERM 't'
#define CMD_INCR 'i'
#define CMD_DECR 'd'
#define CMD_CHLD 'c'


