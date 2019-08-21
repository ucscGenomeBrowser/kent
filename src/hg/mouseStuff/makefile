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
	chainFilter \
	chainMergeSort \
	chainNet \
	chainPreNet \
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
%.all:
	cd $* && echo $* && ${MAKE}

userApps: $(UTILS_APPLIST:%=%.userApp)
%.userApp:
	cd $* && echo $* && $(MAKE)

clean:: $(USE_DIRS:%=%.cleanAll)
%.cleanAll:
	cd $* && echo $* && $(MAKE) clean

clean::
	cd simpleChain && ${MAKE} clean

testAll: $(USE_DIRS:%=%.testAll)

%.testAll:
	@test -s $*/tests/makefile && (echo $*; cd $* && $(MAKE) test) || true

test:: testAll
