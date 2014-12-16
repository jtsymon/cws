#!/usr/bin/env make -f

SRC_DIR=src
TESTS_DIR=tests
PLUGINS_DIR=plugins

CFLAGS+=
LDFLAGS+=-ldl

EXECUTABLE=cws

-include build.Makefile

all: build plugins test

run: build
	./$(EXECUTABLE)

test:
	$(MAKE) -C $(TESTS_DIR) test

plugins:
	$(MAKE) -C $(PLUGINS_DIR)
	
build: $(CORE_OBJECTS) $(OBJECTS) $(EXECUTABLE)

clean:
	$(MAKE) -C $(SRC_DIR) clean
	$(MAKE) -C $(TESTS_DIR) clean
	$(MAKE) -C $(PLUGINS_DIR) clean
	rm -f $(OBJECTS) $(EXECUTABLE)

.PHONY: all run test plugins build clean
