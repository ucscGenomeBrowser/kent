# Build all directories in utils

include ../inc/common.mk

# note: lists such as the following need to be defined *before*
# they are used in the wildcard target rules.  An oddity of gnu make

# define the list of user applications to build

UTILS_APPLIST = \
	bedClip \
	bedGraphToBigWig \
	bedToBigBed \
	bigBedNamedItems \
	bigBedSummary \
	bigBedToBed \
	bigWigAverageOverBed  \
	bigWigInfo \
	bigWigMerge \
	bigWigSummary \
	bigWigToBedGraph \
	bigWigToWig \
	faCount \
	faFrag \
	faOneRecord \
	faPolyASizes \
	faRandomize \
	faSize \
	faSomeRecords \
	faToNib \
	faToTwoBit \
	htmlCheck \
	nibFrag \
	nibSize \
	paraFetch \
	paraSync \
	sizeof \
	stringify \
	textHistogram \
	twoBitDup \
        twoBitInfo \
	twoBitToFa \
	wigCorrelate \
	wigToBigWig

APPS = \
	addCols \
	ave \
	aveCols \
	bedCommonRegions \
	bedGeneParts \
	bedPileUps \
	bedRemoveOverlap \
	bedRestrictToPositions \
	calc \
	catDir \
	catUncomment \
	ccCp \
	chopFaLines \
	colTransform \
	countChars \
	convolve \
	detab \
	endsInLf \
	fastqToFa \
	faAlign \
	faCmp \
	faFilterN \
	faFilter \
	faFlyBaseToUcsc \
	faGapLocs \
	faGapSizes \
	faNcbiToUcsc \
	faNoise \
	faRc \
	faSimplify \
	faSplit \
	faToFastq \
	faToTab \
	faTrans \
	faTrimPolyA \
	faTrimRead \
	findMotif \
	fixCr \
	gapSplit \
	gffPeek \
	git-reports \
	gitTools \
	headRest \
	htmlPics \
	jkUniq \
	lineCount \
	newProg \
	nt4Frag \
	pslLiftSubrangeBlat \
	pslToPslx \
	pslToXa \
	randomLines \
	raSqlQuery \
	raToTab \
	raToLines \
	rmFaDups \
	rowsToCols \
	scaffoldFaToAgp \
	scrambleFa \
	spacedToTab \
	splitFile \
	splitFileByColumn \
	subChar \
	subColumn \
	subs \
	tableSum \
	tailLines \
	textHist2 \
	tickToDate \
	timing \
	toLower \
	toUpper \
	udcCleanup \
	undupFa \
	upper \
	venn \
	verticalSplitSqlTable \
	weedLines \
	wigTestMaker \
	wordLine

# list of utilities to build of interest only at UCSC

UCSC_APPS = \
	aNotB

# list of scripts to install

SCRIPT_FILES = \
        agpToLift \
        bedCollapse.pl \
        iSync \
        lodToBedScore \
	mkMafFrames.pl

# List of all directories to build

ALL_APPS = $(APPS) $(UTILS_APPLIST) $(UCSC_APPS)

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
