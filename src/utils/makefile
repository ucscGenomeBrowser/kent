# Build all directories in utils

include ../inc/common.mk

all:    
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

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
USE_DIRS = $(DIRS)

DIRS = \
	aNotB \
	addCols \
	ave \
	aveCols \
	bedClip \
	bedCommonRegions \
	bedGeneParts \
	bedGraphToBigWig \
	bedRemoveOverlap \
	bedRestrictToPositions \
	bedToBigBed \
	bigBedInfo \
	bigBedSummary \
	bigBedToBed \
	bigWigAverageOverBed \
	bigWigInfo \
	bigWigMerge \
	bigWigSummary \
	bigWigToBedGraph \
	bigWigToWig \
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
	faCount \
	faFilterN \
	faFilter \
	faFlyBaseToUcsc \
	faFrag \
	faGapLocs \
	faGapSizes \
	faNcbiToUcsc \
	faNoise \
	faOneRecord \
	faPolyASizes \
	faRc \
	faSimplify \
	faSize \
	faSomeRecords \
	faSplit \
	faToFastq \
	faToNib \
	faToTab \
	faToTwoBit \
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
	htmlCheck \
	jkUniq \
	lineCount \
	newProg \
	nibFrag \
	nibSize \
	nt4Frag \
	paraFetch \
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
	sizeof \
	spacedToTab \
	splitFile \
	splitFileByColumn \
	stringify \
	subChar \
	subColumn \
	subs \
	tableSum \
	tailLines \
	textHist2 \
	textHistogram \
	tickToDate \
	timing \
	toLower \
	toUpper \
	twoBitInfo \
	twoBitToFa \
	udcCleanup \
	undupFa \
	upper \
	venn \
	weedLines \
	wigCorrelate \
	wigToBigWig \
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
