# Build all directories in utils

all:    cgi utils

utils:  lib
	@for D in $(USE_DIRS) x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && $(MAKE) ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done

lib:
	cd lib && ${MAKE}

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

# Build cgi's

cgi:
	cd lib && ${MAKE}
	cd cartReset && ${MAKE}
	cd das && ${MAKE}
	cd hgBlat && ${MAKE}
	cd hgCoordConv && ${MAKE}
	cd hgGateway && ${MAKE}
	cd hgText && ${MAKE}
	cd hgTracks && ${MAKE}
	cd hgTrackUi && ${MAKE}
	cd hgc && ${MAKE}
	cd cartDump && ${MAKE}

alpha:
	cd lib && ${MAKE}
	cd cartReset && ${MAKE} alpha
	cd das && ${MAKE} alpha
	cd hgBlat && ${MAKE} alpha
	cd hgCoordConv && ${MAKE} alpha
	cd hgGateway && ${MAKE} alpha
	cd hgText && ${MAKE} alpha
	cd hgTracks && ${MAKE} alpha
	cd hgTrackUi && ${MAKE} alpha
	cd hgc && ${MAKE} alpha
	cd cartDump && ${MAKE} alpha

beta:
	cd lib && ${MAKE}
	cd cartReset && ${MAKE} beta
	cd das && ${MAKE} beta
	cd hgBlat && ${MAKE} beta
	cd hgCoordConv && ${MAKE} beta
	cd hgGateway && ${MAKE} beta
	cd hgText && ${MAKE} beta
	cd hgTracks && ${MAKE} beta
	cd hgTrackUi && ${MAKE} beta
	cd hgc && ${MAKE} beta
	cd cartDump && ${MAKE} beta

clean: 
	cd lib && ${MAKE} clean;


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

