#!/usr/bin/env make -f

SRC_DIR=../src
INC_DIR=../include

-include ../build.Makefile

CFLAGS+=-fPIC -I$(INC_DIR)
LDFLAGS+=-shared

all: build

build: $(OBJECTS) $(LIBRARIES)

clean:
	rm -f $(OBJECTS) $(LIBRARIES)

.PHONY: all build clean
