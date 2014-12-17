#!/usr/bin/env make -f

DIRS = $(shell ls -d */)

all: test

test: build
	@failed=0;                              \
	total=0;                                \
	for dir in $(DIRS); do                  \
		echo "*** Running test: $$dir";     \
		make -s -C $$dir test || ((failed++)); \
		((total++));                        \
	done;                                   \
	echo "$$(($$total - $$failed))/$$total tests passed"

build:
	@for dir in $(DIRS); do \
		make -C $$dir build; \
	done

clean:
	@for dir in $(DIRS); do \
		make -C $$dir clean; \
	done

.PHONY: all test build clean
