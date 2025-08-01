# Build all directories in utils

kentSrc = ..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

# note: lists such as the following need to be defined *before*
# they are used in the wildcard target rules.  An oddity of gnu make

# define the list of user applications to build

UTILS_APPLIST = \
	addCols \
	ave \
	aveCols \
	bamToPsl \
	bedClip \
	bedGraphToBigWig \
	bedJoinTabOffset \
	bedToBigBed \
	bigBedInfo \
	bigBedNamedItems \
	bigBedSummary \
	bigBedToBed \
	bedCommonRegions \
	bedGeneParts \
	bedGraphPack \
	bedPileUps \
	bedRemoveOverlap \
	bedRestrictToPositions \
	bigWigAverageOverBed  \
	bigWigCat \
	bigWigCorrelate \
	bigWigCluster \
	bigWigInfo \
	bigWigMerge \
	bigWigSummary \
	bigWigToBedGraph \
	bigWigToWig \
	calc \
	catDir \
	catUncomment \
	chopFaLines \
	colTransform \
	countChars \
	endsInLf \
	clusterMatrixToBarChartBed \
	cpgIslandExt \
	faAlign \
	fastqToFa \
	fastqStatsAndSubsample \
	faCmp \
	faCount \
	faFilterN \
	faFilter \
	faFrag \
	faNoise \
	faOneRecord \
	faPolyASizes \
	faRandomize \
	faRc \
	faSize \
	faSomeRecords \
	faSplit \
	faToFastq \
	faToTab \
	faToTwoBit \
	faTrans \
	findMotif \
	headRest \
	htmlCheck \
	matrixClusterColumns \
	matrixMarketToTsv \
	matrixNormalize \
	matrixToBarChartBed \
	newProg \
	newPythonProg \
	nibFrag \
	nibSize \
	paraFetch \
	paraSync \
	pslMap \
	pslMapPostChain \
	pslPosTarget \
	pslProtToRnaCoords \
	pslRc \
	pslRemoveFrameShifts \
	pslScore \
	pslSpliceJunctions \
	pslSwap \
	pslToPslx \
	randomLines \
	raToTab \
	raToLines \
	rmFaDups \
	rowsToCols \
	sizeof \
	spacedToTab \
	splitFile \
	splitFileByColumn \
	strexCalc \
	stringify \
	subChar \
	subColumn \
	tabFmt \
	tailLines \
	textHistogram \
	tickToDate \
	timing \
	toLower \
	toUpper \
        trackDbIndexBb \
	twoBitDup \
	twoBitInfo \
	twoBitToFa \
	wigCorrelate \
	wigToBigWig \
	wordLine \

# list of utilities to build of interest only at UCSC

UCSC_APPS = \
	addCols \
	aNotB \
	aveCols \
	ccCp \
	convolve \
	detab \
	dnsInfo \
	faFlyBaseToUcsc \
	faGapLocs \
	faGapSizes \
	faNcbiToUcsc \
	faRenameRecords \
	faSimplify \
	faToNib \
	faTrimPolyA \
	faTrimRead \
	fixCr \
	gapSplit \
	gitTools \
	gffPeek \
	git-reports \
	htmlPics \
	jkUniq \
	lineCount \
	nt4Frag \
	pslToXa \
	scrambleFa \
	subs \
	tabToMmHash \
	tableSum \
	textHist2 \
	udcCleanup \
	undupFa \
	upper \
	vcfFilter \
	venn \
	verticalSplitSqlTable \
	weedLines

# list of scripts to install

USER_APP_SCRIPTS = \
	bedJoinTabOffset.py \
	expMatrixToBarchartBed \
	barChartMaxLimit \
	ucscApiClient \
	tdbRename \
	tdbSort \
	webSync \
	chromToUcsc \
	bigHeat \
	bigGuessDb

SCRIPT_FILES = \
	agpToLift \
	apacheLogParse \
	bedCollapse.pl \
	bedFixBlockOverlaps \
	bedOverlapMerge \
	bedSingleCover.pl \
	bedBetween \
	doLocusName \
	extToolsChromeParse \
	iSync \
	lodToBedScore \
	matrixMerge \
	mkMafFrames.pl \
	patSeqFilterBulkAndAnnotate \
	patSeqSummarizeDocs \
	pslProtCnv \
	pslSameGene \
	tabRepl \
	uniprotToTab \
	ggTables \
	ggGeneClasses \
	ggGpmlToTab \
	ggKgmlToTab \
	ggMsrToTab \
	ggPidToTab \
	ggPpiToTab \
	ggSpfToTab \
	bedNamePickFirst \
	bedBestTranscript \
	genePredToBed \
	cgiSmokeTest \
	phegenToBed \
	bigMaxPercDiff \
	cancerMafToBigBed \
	pq \
	bedAppendPaperInfo \
	googleSheetToBigBed \
	doClinvarLift \
	browserRace \
	${USER_APP_SCRIPTS}

# List of all directories to build

ALL_APPS = $(UTILS_APPLIST) $(UCSC_APPS)

# all: builds all utilities here

all: $(ALL_APPS:%=%.all) $(USER_APP_SCRIPTS:%=%.userAppScript)

%.all:
	cd $* && echo $* && $(MAKE)

# userApps: builds the user application specific programs
.PHONY: userApps
userApps: $(UTILS_APPLIST:%=%.userApp) $(USER_APP_SCRIPTS:%=%.userAppScript)

%.userApp:
	cd $* && echo $* && $(MAKE)

%.userAppScript:
	@rm -f ${DESTDIR}${BINDIR}/$*
	@if test -d $* ; \
	then \
	    chmod +x $*/$*; \
	    echo cp -p $*/$* ${DESTDIR}${BINDIR}; \
	    cp -p $*/$* ${DESTDIR}${BINDIR}; \
	else \
	    chmod +x $*; \
	    echo cp -p $*/$* ${DESTDIR}${BINDIR}; \
	    cp -p $* ${DESTDIR}${BINDIR}/$*; \
	fi

scripts: $(SCRIPT_FILES:%=%.scripts)

%.scripts:
	@rm -f ${SCRIPTS}/$*
	@if test -d $* ; \
	then \
	    chmod +x $*/$*; \
	    echo cp -p $*/$* ${SCRIPTS}; \
	    cp -p $*/$* ${SCRIPTS}; \
	else \
	    chmod +x $*; \
	    echo cp -p $* ${SCRIPTS}; \
	    cp -p $* ${SCRIPTS}; \
	fi

testAll: $(ALL_APPS:%=%.testAll)

%.testAll:
	@test -s $*/tests/makefile && (echo $*; cd $* && $(MAKE) test) || true

test: testAll
	cd faToTwoBit && ${MAKE} test
	cd twoBitToFa && ${MAKE} test
	cd twoBitInfo && ${MAKE} test
	cd bedGeneParts && ${MAKE} test

clean: $(ALL_APPS:%=%.clean)
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm

%.clean:
	cd $* && echo $* && $(MAKE) clean

BAD_DIRS= \
	ccCpLock \
	est2genomeToPsl \
	geneStarts \
	jimgrep \
	jkShell \
	wigAsciiToBinary \
	cCp
