# Build txGraph source
include ../../inc/common.mk

all:    
	@for D in $(DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE)  ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

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
