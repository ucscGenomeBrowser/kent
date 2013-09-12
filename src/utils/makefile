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
	bedClip \
	bedGraphToBigWig \
	bedToBigBed \
	bigBedInfo \
	bigBedNamedItems \
	bigBedSummary \
	bigBedToBed \
	bedCommonRegions \
	bedGeneParts \
	bedPileUps \
	bedRemoveOverlap \
	bedRestrictToPositions \
	bigWigAverageOverBed  \
	bigWigCorrelate \
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
	fastqToFa \
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
	gitTools \
	headRest \
	htmlCheck \
	newProg \
	nibFrag \
	nibSize \
	paraFetch \
	paraSync \
	pslLiftSubrangeBlat \
	pslToPslx \
	randomLines \
	raSqlQuery \
	raToTab \
	raToLines \
	rmFaDups \
	rowsToCols \
	sizeof \
	spacedToTab \
	splitFile \
	splitFileByColumn \
	stringify \
	subChar \
	subColumn \
	tailLines \
	textHistogram \
	tickToDate \
	timing \
	toLower \
	toUpper \
	twoBitDup \
        twoBitInfo \
	twoBitToFa \
	wigCorrelate \
	wigToBigWig \
	wordLine

# list of utilities to build of interest only at UCSC

UCSC_APPS = \
	addCols \
	aNotB \
	aveCols \
	ccCp \
	convolve \
	detab \
	endsInLf \
	faAlign \
	faFlyBaseToUcsc \
	faGapLocs \
	faGapSizes \
	faNcbiToUcsc \
	faSimplify \
	faToNib \
	faTrimPolyA \
	faTrimRead \
	fixCr \
	gapSplit \
	gffPeek \
	git-reports \
	htmlPics \
	jkUniq \
	lineCount \
	nt4Frag \
	pslToXa \
	scaffoldFaToAgp \
	scrambleFa \
	subs \
	tableSum \
	textHist2 \
	udcCleanup \
	undupFa \
	upper \
	venn \
	verticalSplitSqlTable \
	weedLines \
	wigTestMaker

# list of scripts to install

SCRIPT_FILES = \
        agpToLift \
        bedCollapse.pl \
        iSync \
        lodToBedScore \
	mkMafFrames.pl

# List of all directories to build

ALL_APPS = $(UTILS_APPLIST) $(UCSC_APPS)

# all: builds all utilities here

all: $(ALL_APPS:%=%.all)

%.all:
	cd $* && echo $* && $(MAKE)

# userAppsB: builds the user application specific programs
# it would have been nice to be able to use the target name 'userApps'
# here, but there is a directory by that name, and thus it always exists
# and so the target does nothing.

userAppsB: $(UTILS_APPLIST:%=%.userApps)

%.userApps:
	cd $* && echo $* && $(MAKE)

scripts: $(SCRIPT_FILES:%=%.scripts)
	chmod +x genePredToBed/genePredToBed;
	rm -f ${SCRIPTS}/genePredToBed;
	echo cp -p genePredToBed/genePredToBed ${SCRIPTS};
	@cp -p genePredToBed/genePredToBed ${SCRIPTS};

%.scripts:
	chmod +x $*
	rm -f ${SCRIPTS}/$*
	echo cp -p $* ${SCRIPTS}
	cp -p $* ${SCRIPTS}

alpha: scripts all

test:
	cd faToTwoBit && ${MAKE} test
	cd twoBitToFa && ${MAKE} test
	cd twoBitInfo && ${MAKE} test
	cd pslLiftSubrangeBlat && ${MAKE} test
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
	dnsInfo \
	wigAsciiToBinary \
	cCp
