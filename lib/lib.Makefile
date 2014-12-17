NAME ?= $(shell basename $(CURDIR))
LIB := ../$(NAME).so

INCLUDE=../../include

CFLAGS+=-fPIC -I$(INCLUDE)
LDFLAGS+=-shared -Llib

all: build

build: prepare $(OBJECTS) $(LIB)

prepare:
	ln -sf ../../lib .

clean:
	rm -f $(OBJECTS) $(LIB)

-include ../../build.Makefile

.PHONY: all prepare build clean
