# Build all directories in makeDb 

include ../../inc/common.mk

BADDIRS = \
        correctEst

# List of directories to build for userApps
UTILS_APPLIST = \
	axtChain \
	axtSort \
	axtSwap \
	axtToMaf \
	axtToPsl \
	chainAntiRepeat \
	chainMergeSort \
	chainPreNet \
	chainSort \
	chainSplit \
	chainStitchId \
	chainSwap \
	chainToAxt \
	chainToPsl \
	lavToAxt \
	lavToPsl \
	mafCoverage \
	mafFetch \
	mafRanges \
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
	bedCoverage \
	calcGap \
	chainDbToFile \
	chainFilter \
	chainNet \
	chainStats \
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

clean::
	cd simpleChain && ${MAKE} clean
