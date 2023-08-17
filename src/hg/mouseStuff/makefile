# Build all directories in makeDb 

kentSrc = ../..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

BADDIRS = \
        correctEst

# List of directories to build for userApps
UTILS_APPLIST = \
	axtChain \
	axtSort \
	axtSwap \
	axtToMaf \
	axtToPsl \
	bedCoverage \
	chainAntiRepeat \
	chainBridge \
	chainCleaner \
	chainFilter \
	chainMergeSort \
	chainNet \
	chainPreNet \
	chainScore \
	chainSort \
	chainSplit \
	chainStitchId \
	chainSwap \
	chainToAxt \
	chainToPsl \
	chainToPslBasic \
	lavToAxt \
	lavToPsl \
	mafCoverage \
	mafFetch \
	mafRanges \
	mafNoAlign \
	mafToAxt \
	mafToPsl \
	netChainSubset \
	netClass \
	netFilter \
	netSplit \
	netSyntenic \
	netToAxt \
	netToBed \
	pslMrnaCover

# List of directories to build at UCSC
DIRS = \
	axtAndBed \
	axtBest \
	axtCalcMatrix \
	axtDropOverlap \
	axtDropSelf \
	axtFilter \
	axtIndex \
	axtPretty \
	axtQueryCount \
	axtRecipBest \
	axtRescore \
	axtSplitByTarget \
	axtToBed \
	axtToChain \
	calcGap \
	chainDbToFile \
	gcForBed \
	knownVsBlat \
	libScan \
	mousePoster \
	netStats \
	netToBedWithId \
        orthologBySynteny \
	phToPsl \
	regionPicker \
	rikenBestInCluster \
	simpleChain \
	splitSim \
	subsetAxt \
	subsetTraces \
	whyConserved 

# List of directories to build
USE_DIRS = $(DIRS) $(UTILS_APPLIST)

all: ${USE_DIRS:%=%.all}
# kent-core doesn't contain all utils, so tolerate missing directories
%.all:
	if test -d $* ; then cd $* && echo $* && $(MAKE) ; fi

userApps: $(UTILS_APPLIST:%=%.userApp)
# kent-core doesn't contain all utils, so tolerate missing directories
%.userApp:
	if test -d $* ; then cd $* && echo $* && $(MAKE) ; fi

clean:: $(USE_DIRS:%=%.cleanAll)
%.cleanAll:
	if test -d $* ; then cd $* && echo $* && $(MAKE) clean ; fi

clean::
	cd simpleChain && ${MAKE} clean

testAll: $(USE_DIRS:%=%.testAll)

%.testAll:
	@test -s $*/tests/makefile && (echo $*; cd $* && $(MAKE) test) || true

test:: testAll
