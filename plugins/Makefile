#!/usr/bin/env make -f

SRC_DIR=../src

-include ../build.Makefile

CFLAGS+=-fPIC
LDFLAGS+=-shared

all: build

build: $(OBJECTS) $(LIBRARIES)

clean:
	rm -f $(OBJECTS) $(LIBRARIES)

.PHONY: all build clean
