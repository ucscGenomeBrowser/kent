kentSrc = ../..
# Build txGraph source
include ../../inc/common.mk

# List of directories to build
DIRS = \
    txCdsBadBed \
    txCdsCluster \
    txCdsEvFromBed \
    txCdsEvFromBorf \
    txCdsEvFromProtein \
    txCdsEvFromRna \
    txCdsGoodBed \
    txCdsOrfInfo \
    txCdsOrtho \
    txCdsPick \
    txCdsPredict \
    txCdsRaExceptions \
    txCdsRefBestEvOnly \
    txCdsRepick \
    txCdsSuspect \
    txCdsSvmInput \
    txCdsToGene \
    txCdsWeed \
    txInfoAssemble

USE_DIRS = $(DIRS)

all: $(USE_DIRS:%=%.all)

%.all:
	( cd $* && echo $* && $(MAKE) )

clean:: $(USE_DIRS:%=%.clean)

%.clean::
	( cd $* && echo $* && $(MAKE) clean )

compile:: $(USE_DIRS:%=%.compile)

%.compile::
	( cd $* && echo $* && $(MAKE) compile )
