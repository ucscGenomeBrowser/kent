kentSrc = ..
include ../inc/common.mk
include ./fuse.mk

FUSE_DIRS = \
	udcFuse

all: $(FUSE_DIRS:%=%_all)
%_all:
	$(MAKE) -C $*

alpha: all

compile: $(FUSE_DIRS:%=%_compile)
%_compile:
	$(MAKE) -C $* compile


# clean would be redundant with top-level makefile's (find & rm .o's)

clean::  $(FUSE_DIRS:%=%_clean)
%_clean:
	$(MAKE) -C $* clean
