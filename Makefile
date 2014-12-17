#!/usr/bin/env make -f

INC_DIR=include
TESTS_DIR=tests
LIB_DIR=lib

CFLAGS+=-I$(INC_DIR)
LDFLAGS+=-l:lib/core.so

EXECUTABLE=cws

-include build.Makefile

all: build test

run: build
	./$(EXECUTABLE)

test: lib
	$(MAKE) -C $(TESTS_DIR) test

lib:
	$(MAKE) -C $(LIB_DIR)
	
build: lib $(OBJECTS) $(EXECUTABLE)

clean:
	$(MAKE) -C $(TESTS_DIR) clean
	$(MAKE) -C $(LIB_DIR) clean
	rm -f $(OBJECTS) $(EXECUTABLE)

.PHONY: all run test lib build clean
