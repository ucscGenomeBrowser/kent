# Build all directories in utils

BROWSER_BINS=cartReset das hgBlat hgCoordConv hgGateway hgText hgTracks \
	hgTrackUi hgc cartDump

all::    cgi utils

utils:
	cd lib && ${MAKE}
	@for D in $(USE_DIRS) x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && $(MAKE) ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done

# List of directories to build
USE_DIRS = $(DIRS)

BAD_DIRS = \
	altGraph \
	autoXml \
	cgiExample \
	checkAgpAndFa \
	geneBounds \
	geneOverlap \
	hgBatch \
	makePai \
	makeSeqs \
	mouseStuff \
	ooCloneOverlaps \
	oneShot \
	orthoMap \
	pslCrudeCmp \
	synMap \
	wikiPlot 

# Compile only, the cgi's for those builds without write access
#	to apache cgi-bin directories  (this compiles and links executables)

compile:
	cd lib && ${MAKE}
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE} compile;) \
	done

# Build cgi's
#   cgi: Destination for the binaries is /usr/local/apache/cgi-bin-${USER}

cgi:
	cd lib && ${MAKE}
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE};) \
	done

#   alpha: Destination for the binaries is /usr/local/apache/cgi-bin/

alpha:
	cd lib && ${MAKE}
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE} alpha;) \
	done

#   beta: Destination for the binaries is /usr/local/apache/cgi-bin-beta/

beta:
	cd lib && ${MAKE}
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE} beta;) \
	done

# install: Destination for the binaries is ${DESTDIR}/usr/local/apache/cgi-bin

install:
	cd lib && ${MAKE}
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE} install;) \
	done

clean: 
	cd lib && ${MAKE} clean
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE} clean;) \
	done


DIRS = \
	affyTransciptome \
	agpCloneCheck \
	agpCloneList \
	agpToFa \
	agpToGl \
	autoSql \
	barger \
	bedIntersect \
	bedSort \
	blast2psl \
	borfBig \
	cartDump \
	cartReset \
	checkDbSync \
	checkGoldDupes \
	checkN \
	checkNt \
	checkOut \
	checkPsl \
	checkQa \
	checkYbr \
	cmParse \
	cmpMap \
	ctgFaToFa \
	ctgToChromFa \
	das \
	fakeFinContigs \
	fakeOut \
	featureBits \
	ffaToFa \
	foldDb \
	foldGfx \
	fqToQa \
	fqToQac \
	fragPart \
	g2gOverlap \
	g2gSeqOverlap \
	gapper \
	gbOneAcc \
	gbToFaRa \
	getFeatDna \
	getRna \
	getRnaPred \
	goldToAgp \
	gpStats \
	gpToGtf \
	gpcrParser \
	gsBig \
	hgBlat \
	hgCoordConv \
	hgGateway \
	hgText \
	hgTrackUi \
	hgTracks \
	hgc \
	hgsql \
	intronEnds \
	lib \
	liftAgp \
	liftFrags \
	liftOver \
	liftUp \
	makeDb \
	maskOutFa \
	mdToNcbiLift \
	mendMap \
	mrnaToGene \
	nci60 \
	ntGoldGl \
	ooChains \
	ooCloneEnds \
	ooCloneInfo \
	ooCloneSizes \
	ooCloneSpan \
	ooGreedy \
	ooLiftSpec \
	ooNtInput \
	ooSplitFins \
	ooTester \
	ooWeedZero \
	patCount \
	psLayout \
	pslCat \
	pslCheck \
	pslCoverage \
	pslDropOverlap \
	pslFilter \
	pslGlue \
	pslHitPercent \
	pslIntronsOnly \
	pslPretty \
	pslReps \
	pslSimp \
	pslSort \
	pslSortAcc \
	pslSwap \
	pslUnpile \
	qaGold \
	qaToQac \
	qacToQa \
	relPairs \
	reviewSanity \
	sage \
	scanRa \
	secure \
	semiNorm \
	sim4big \
	splitFa \
	splitFaIntoContigs \
	test \
	trfBig \
	uniqSize

