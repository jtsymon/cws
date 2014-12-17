CC=gcc
CFLAGS+=-c -Wall
LDFLAGS+=

SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

$(LIB): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

$(BIN): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
