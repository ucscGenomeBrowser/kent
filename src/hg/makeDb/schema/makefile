include ../../../inc/common.mk
# Build all directories in makeDb 

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

test::
	cd joinerCheck && ${MAKE} test

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	joinerCheck \
	joinableFields \
	makeTableDescriptions \
	dbFindFieldsWith \
	dbSnoop
	
