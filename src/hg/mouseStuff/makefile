# Build all directories in makeDb 

all:
	@for D in ${USE_DIRS} x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && ${MAKE} ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

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

clean::
	@for D in ${DIRS}; do \
		(cd $${D} && ${MAKE} clean;) \
	done
