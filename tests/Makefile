#!/usr/bin/env make -f

SRC_DIR=../src

-include ../build.Makefile

CFLAGS+=-I$(SRC_DIR)

all: test

test: $(CORE_OBJECTS) $(OBJECTS) $(EXECUTABLES)
	@PATH=`pwd`;                            \
	cd ../;                                 \
	failed=0;                               \
	total=0;                                \
	for test in $(EXECUTABLES); do          \
		echo "*** Running test: '$$test'";  \
		$$test || ((failed++));             \
		((total++));                        \
	done;                                   \
	echo "$$(($$total - $$failed))/$$total tests passed"


clean:
	rm -f $(OBJECTS) $(EXECUTABLES)

.PHONY: all test clean
