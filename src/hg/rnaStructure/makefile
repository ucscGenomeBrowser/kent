kentSrc = ../..
include ../../inc/common.mk
# Build all directories in makeDb 

# List of directories to build
DIRS = \
    hgLoadRnaFold \
    rnaFoldBig \
    utrFa 

USE_DIRS = ${DIRS}

all: $(USE_DIRS:%=%.all)

%.all:
	( cd $* && echo $* && ${MAKE} )

clean:: $(USE_DIRS:%=%.clean)

%.clean::
	( cd $* && echo $* && ${MAKE} clean )

compile:: $(USE_DIRS:%=%.compile)

%.compile::
	( cd $* && echo $* && ${MAKE} compile )
