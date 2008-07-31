# Build all directories in makeDb 

include ../../inc/common.mk

# List of directories to build
USE_DIRS = ${DIRS}

BADDIRS = \
        correctEst

DIRS = \
	axtAndBed \
	axtBest \
	axtCalcMatrix \
	axtChain \
	axtDropOverlap \
	axtDropSelf \
	axtFilter \
	axtIndex \
	axtPretty \
	axtQueryCount \
	axtRecipBest \
	axtRescore \
	axtSort \
	axtSplitByTarget \
	axtSwap \
	axtToBed \
	axtToChain \
	axtToMaf \
	axtToPsl \
	bedCoverage \
	calcGap \
	chainAntiRepeat \
	chainDbToFile \
	chainFilter \
	chainMergeSort \
	chainNet \
	chainPreNet \
	chainSort \
	chainSplit \
	chainStats \
	chainStitchId \
	chainSwap \
	chainToAxt \
	chainToPsl \
	gcForBed \
	knownVsBlat \
	lavToAxt \
	lavToPsl \
	libScan \
	mafCoverage \
	mafFetch \
	mafRanges \
	mafToAxt \
	mousePoster \
	netChainSubset \
	netClass \
	netFilter \
	netSplit \
	netStats \
	netSyntenic \
	netToAxt \
	netToBed \
	netToBedWithId \
	orthologBySynteny \
	phToPsl \
	pslMrnaCover \
	regionPicker \
	rikenBestInCluster \
	simpleChain \
	splitSim \
	subsetAxt \
	subsetTraces \
	whyConserved 

all: ${USE_DIRS:%=%.all}
%.all:
	( cd $* && echo $* && ${MAKE} )

clean:: ${USE_DIRS:%=%.all}
	(cd $* && ${MAKE} clean)
