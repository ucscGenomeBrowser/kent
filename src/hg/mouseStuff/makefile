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
	axtAndBed \
	axtBest \
	axtCalcMatrix \
	axtChain \
	chainToPsl \
	axtDropOverlap \
	axtDropSelf \
	axtFilter \
	axtIndex \
	chainInfo \
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
	blastzPaper \
	blastzScripts \
	calcGap \
	chainFilter \
	chainMergeSort \
	chainNet \
	chainPreNet \
	chainSort \
	chainSplit \
	chainSwap \
	correctEst \
	gcForBed \
	ggcPic \
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
	subsetRef \
	subsetTraces \
	whyConserved 
