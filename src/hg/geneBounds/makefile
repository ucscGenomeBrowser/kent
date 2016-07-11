kentSrc = ../..
include ../../inc/common.mk
# Build all directories in makeDb 

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	assessLibs \
	averagExp \
	axtForEst \
	bedCons \
	checkableBorf \
	clusterGenes \
	clusterPsl \
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


all: $(USE_DIRS:%=%.all)

lib.all:
	cd lib && $(MAKE)

# make sure lib is built first, even in a parallel make

%.all: lib.all
	cd $* && echo $* && $(MAKE)

test::
	cd clusterGenes && ${MAKE} test

clean:: $(USE_DIRS:%=%.clean)

%.clean::
	cd $* && echo $* && $(MAKE) clean
