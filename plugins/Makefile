#!/usr/bin/env make -f

-include ../build.Makefile

CFLAGS+=-fPIC
LDFLAGS+=-shared

all: build

build: $(OBJECTS) $(LIBRARIES)

clean:
	rm -f $(OBJECTS) $(LIBRARIES)

.PHONY: all clean
