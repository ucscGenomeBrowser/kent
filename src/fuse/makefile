include ../inc/common.mk
include ./fuse.mk

FUSE_DIRS = \
	udcFuse

all:    
	@for D in $(FUSE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

alpha: all

# clean would be redundant with top-level makefile's (find & rm .o's)

clean::
	rm -f ${O}
