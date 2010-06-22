include ../../../inc/common.mk
# Build all directories in makeDb 

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
    mysqlEscape


all: $(USE_DIRS:%=%.all)

%.all:
	( cd $* && echo $* && $(MAKE) )

clean:: $(USE_DIRS:%=%.clean)

%.clean::
	( cd $* && echo $* && $(MAKE) clean )

test::
	cd clusterGenes && ${MAKE} test
