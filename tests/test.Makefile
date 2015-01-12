NAME := $(shell basename $(CURDIR)).test
BIN := ../$(NAME)

INCLUDE=../../include

CFLAGS+=-I$(INCLUDE)
LDFLAGS+=-L.

all: test

test: build
	PATH=`pwd`/..; \
	cd ../..;      \
	$(NAME)

build: prepare $(OBJECTS) $(BIN)

prepare:
	ln -sf ../../lib .

clean:
	rm -f $(OBJECTS) $(BIN)

-include ../../build.Makefile

.PHONY: all test prepare build clean
