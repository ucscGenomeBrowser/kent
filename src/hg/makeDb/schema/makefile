kentSrc = ../../..
include ../../../inc/common.mk
# Build all directories in makeDb 

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	joinerCheck \
	joinerRoute \
	joinableFields \
	makeTableDescriptions \
	dbFindFieldsWith \
	dbSnoop

all: $(USE_DIRS:%=%.all)

%.all:
	( cd $* && echo $* && $(MAKE) )

clean:: $(USE_DIRS:%=%.clean)

%.clean::
	( cd $* && echo $* && $(MAKE) clean )

test::
	cd joinerCheck && ${MAKE} test
