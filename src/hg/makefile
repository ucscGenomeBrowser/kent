# Build all directories in utils

BROWSER_BINS=cartReset das hgBlat hgCoordConv hgGateway hgGene hgText hgTracks \
	hgTrackUi hgc cartDump near/hgNear

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
	cgiExample \
	geneOverlap \
	hgBatch \
	makeSeqs \
	oneShot \
	orthoMap \
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
	- find . -name \*.o -print | xargs rm


DIRS = \
	affyTransciptome \
	agpCloneCheck \
	agpCloneList \
	agpToFa \
	agpToGl \
	autoSql \
	autoXml \
	bedIntersect \
	bedSort \
	borfBig \
	cartDump \
	cartReset \
	ctgFaToFa \
	ctgToChromFa \
	das \
	fakeFinContigs \
	fakeOut \
	featureBits \
	ffaToFa \
	fqToQa \
	fqToQac \
	fragPart \
	gbOneAcc \
	gbToFaRa \
	geneBounds \
	getFeatDna \
	getRna \
	getRnaPred \
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
	hgsqldump \
	intronEnds \
	lib \
	liftAgp \
	liftFrags \
	liftOver \
	liftUp \
	makeDb \
	maskOutFa \
	mdToNcbiLift \
	mouseStuff \
	mrnaToGene \
	nci60 \
	patCount \
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
	qaToQac \
	qacToQa \
	ratStuff \
	relPairs \
	recycleDb \
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

