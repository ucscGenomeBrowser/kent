# Build all directories in makeDb 

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	averagExp \
	axtForEst \
	bedCons \
	checkableBorf \
	clusterRna \
	dnaMotifFind \
	eisenInput \
	emblMatrixToMotif \
	expToRna \
	lib \
	liftPromoHits \
	motifLogo \
	motifSig \
	orf \
	orfStats \
	polyInfo \
	promoSeqFromCluster \
	raToCds \
	selectTrainedHits \
	twinOrf \
	twinOrf2 \
	twinOrf3 \
	twinOrfStats \
	twinOrfStats2 \
	twinOrfStats3 

BADDIRS = \
	assessLibs \
	estLibStats \
	iriToControlTable \
	iriToDnaMotif 
