#!/usr/bin/env make -f

DIRS = $(shell ls -d */)

all:
	@for dir in $(DIRS); do \
		make -C $$dir; \
	done

clean:
	@for dir in $(DIRS); do \
		make -C $$dir clean; \
	done
