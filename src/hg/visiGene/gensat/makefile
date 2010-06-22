include ../../../inc/common.mk
# Build all directories in makeDb 

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
    lib \
    gensatFixFull \
    gensatFtpImages \
    gensatFtpList \
    gensatRandomXml 

#    vgLoadGensat \	// needs work to compile
#    gensatImageDownload \	// needs work to compile

all: $(USE_DIRS:%=%.all)

%.all:
	( cd $* && echo $* && $(MAKE) )

clean:: $(USE_DIRS:%=%.clean)

%.clean::
	( cd $* && echo $* && $(MAKE) clean )

test::
	cd clusterGenes && ${MAKE} test

