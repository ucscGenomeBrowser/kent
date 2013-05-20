# Build all directories in utils

include ../inc/common.mk

# all: builds all utilities here

all:    
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

# userApps: builds on user application specific programs

userApps: $(UTILS_APPLIST:%=%.userApps)

%.userApps:
	( cd $* && echo $* && $(MAKE) )

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
	    rm -f ${SCRIPTS}/$${F}; \
	    echo cp -p $$F ${SCRIPTS}; \
	    cp -p $$F ${SCRIPTS}; \
	 done
	chmod +x genePredToBed/genePredToBed;
	rm -f ${SCRIPTS}/genePredToBed;
	echo cp -p genePredToBed/genePredToBed ${SCRIPTS};
	@cp -p genePredToBed/genePredToBed ${SCRIPTS};

alpha: scripts all

test:
	cd faToTwoBit && ${MAKE} test
	cd twoBitToFa && ${MAKE} test
	cd twoBitInfo && ${MAKE} test
	cd pslLiftSubrangeBlat && ${MAKE} test
	cd bedGeneParts && ${MAKE} test

clean:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) clean ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm

SCRIPT_FILES = \
        agpToLift \
        bedCollapse.pl \
        iSync \
        lodToBedScore \
	mkMafFrames.pl

# List of directories to build
USE_DIRS = $(DIRS) $(UTILS_APPLIST) $(UCSC_DIRS)

UCSC_DIRS = \
	aNotB

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
	bigWigToWig\
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

DIRS = \
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

BAD_DIRS= \
	ccCpLock \
	est2genomeToPsl \
	geneStarts \
	jimgrep \
	jkShell \
	dnsInfo \
	wigAsciiToBinary \
	cCp
