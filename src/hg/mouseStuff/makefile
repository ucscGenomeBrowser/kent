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

BADDIRS = \
        correctEst

DIRS = \
	axtAndBed \
	axtBest \
	axtCalcMatrix \
	axtChain \
	chainToPsl \
	axtDropOverlap \
	axtDropSelf \
	axtFilter \
	axtIndex \
	axtPretty \
	axtQueryCount \
	axtRecipBest \
	axtSort \
	axtSplitByTarget \
	axtSwap \
	axtToBed \
	axtToMaf \
	axtToPsl \
	bedCoverage \
	calcGap \
	chainFilter \
	chainMergeSort \
	chainNet \
	chainPreNet \
	chainSort \
	chainSplit \
	chainSwap \
	gcForBed \
	knownVsBlat \
	lavToAxt \
	lavToPsl \
	libScan \
	mmUnmix \
	mousePoster \
	netChainSubset \
	netClass \
	netFilter \
	netSplit \
	netStats \
	netSyntenic \
	netToAxt \
	netToBed \
	orthologBySynteny \
	phToPsl \
	pslMrnaCover \
	regionPicker \
	rikenBestInCluster \
	splitSim \
	subsetAxt \
	subsetTraces \
	whyConserved 
