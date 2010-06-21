include ../../inc/common.mk

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
    microscope \
    microscopeLoad 

# Needs work to compile
#	vgPrepImage \

all: $(USE_DIRS:%=%.all)

%.all:
	( cd $* && echo $* && $(MAKE) )

clean:: $(USE_DIRS:%=%.clean)

%.clean::
	( cd $* && echo $* && $(MAKE) clean )
