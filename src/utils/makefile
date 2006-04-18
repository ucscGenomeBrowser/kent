# Build all directories in utils

include ../inc/common.mk

all:    
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) 'HG_WARN=${HG_WARN_ERR}' ) ;\
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
	cp -p genePredToBed/genePredToBed ${SCRIPTS};
	cd cvsTools && ${MAKE} scripts

alpha: scripts all

test:
	cd faToTwoBit && ${MAKE} test
	cd twoBitToFa && ${MAKE} test
	cd twoBitInfo && ${MAKE} test

clean:
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm

SCRIPT_FILES = \
        agpToLift \
	blastz-make-joblist \
	blastz-normalizeLav \
	blastz-run-ucsc \
	doBlastzChainNet.pl \
	doHgNearBlastp.pl \
	dumpDb.pl \
	HgAutomate.pm \
        iSync \
        lodToBedScore \
	mkMafFrames.pl \
	partitionSequence.pl

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	aNotB \
	addCols \
	ave \
	aveCols \
	calc \
	catDir \
	catUncomment \
	ccCp \
	chopFaLines \
	countChars \
	convolve \
	cvsTools \
	detab \
	endsInLf \
	est2genomeToPsl \
	faAlign \
	faCmp \
	faCount \
	faFilterN \
	faFilter \
	faFlyBaseToUcsc \
	faFrag \
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
	faToNib \
	faToTab \
	faToTwoBit \
	faTrans \
	faTrimPolyA \
	faTrimRead \
	findMotif \
	fixCr \
	gffPeek \
	gtfToGenePred \
	htmlPics \
	htmlCheck \
	jkUniq \
	lineCount \
	newProg \
	nibFrag \
	nibSize \
	nt4Frag \
	pslToXa \
	randomLines \
	rmFaDups \
	scaffoldFaToAgp \
	scrambleFa \
	spacedToTab \
	splitFile \
	stringify \
	subChar \
	subs \
	tableSum \
	textHist2 \
	textHistogram \
	tickToDate \
	toLower \
	toUpper \
	twoBitInfo \
	twoBitToFa \
	undupFa \
	upper \
	venn \
	wordLine

BAD_DIRS= \
	ccCpLock \
	geneStarts \
	jimgrep \
	jkShell \
	dnsInfo \
	wigAsciiToBinary \
	cCp
