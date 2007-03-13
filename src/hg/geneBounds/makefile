include ../../inc/common.mk
# Build all directories in makeDb 

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

test::
	cd clusterGenes && ${MAKE} test

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	assessLibs \
	averagExp \
	axtForEst \
	bedCons \
	checkableBorf \
	clusterGenes \
	clusterRna \
	dnaMotifFind \
	eisenInput \
	emblMatrixToMotif \
	estLibStats \
	expToRna \
	iriToControlTable \
	iriToDnaMotif \
	lib \
	liftPromoHits \
	motifLogo \
	motifSig \
	orf \
	orfStats \
	polyInfo \
	promoSeqFromCluster \
	raToCds \
	twinOrf \
	twinOrf2 \
	twinOrf3 \
	twinOrfStats \
	twinOrfStats2 \
	twinOrfStats3 
