CC=gcc
CFLAGS+=-c -Wall
LDFLAGS+=-ldl

SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLES=$(SOURCES:.c=)
LIBRARIES=$(SOURCES:.c=.so)

CORE_SOURCES=$(wildcard ./$(SRC_DIR)/*.c)
CORE_OBJECTS=$(CORE_SOURCES:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

%.so: %.o
	$(CC) $(LDFLAGS) $< -o $@

%: %.o
	$(CC) $(LDFLAGS) $< $(CORE_OBJECTS) -o $@
