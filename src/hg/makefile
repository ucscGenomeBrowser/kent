# Build all directories in utils

BROWSER_BINS=cartReset das hgBlat hgCoordConv hgGateway hgGene hgPcr hgTables hgText hgTracks \
	hgTrackUi hgc cartDump near/hgNear protein/pbGateway protein/pbTracks protein/pbGlobal \
	hgLiftOver hgc/cgiClustalw

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
USE_DIRS = $(UTIL_DIRS)

BAD_DIRS = \
	altGraph \
	cgiExample \
	geneOverlap \
	hgBatch \
	makeSeqs \
	oneShot \
	synMap \
	encode \
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

clean::
	cd lib && ${MAKE} clean
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE} clean;) \
	done
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm

clean_utils::
	@for D in ${UTIL_DIRS}; do \
		(cd $${D} && ${MAKE} clean;) \
	done

test::
	cd lib && ${MAKE} test
	cd blastToPsl && ${MAKE} test
	cd embossToPsl && ${MAKE} test
	cd geneBounds && ${MAKE} test
	cd hgGetAnn && ${MAKE} test
	cd liftUp && ${MAKE} test
	cd mrnaToGene && ${MAKE} test
	cd overlapSelect && ${MAKE} test
	cd pslMap && ${MAKE} test
	cd spideyToPsl && ${MAKE} test

UTIL_DIRS = \
	affyTransciptome \
	agpCloneCheck \
	agpCloneList \
	agpAllToFaFile \
	agpToFa \
	agpToGl \
	autoSql \
	autoXml \
	bedIntersect \
	bedSort \
	blastToPsl \
	borfBig \
	checkHgFindSpec \
	checkTableCoords \
	ctgFaToFa \
	ctgToChromFa \
	embossToPsl \
        encode \
	fakeFinContigs \
	fakeOut \
	featureBits \
	ffaToFa \
	fishClones \
	fqToQa \
	fqToQac \
	fragPart \
	gbGetEntries \
	gbOneAcc \
	gbToFaRa \
	geneBounds \
	genePredCheck \
	genePredToGtf \
	getFeatDna \
	getRna \
	getRnaPred \
	gpStats \
	gpToGtf \
	gpcrParser \
	gsBig \
	hgTablesTest \
	hgChroms \
	hgGetAnn \
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
	near/hgCeOrfToGene \
	near/hgClusterGenes \
	near/hgEmblProtLinks \
	near/hgExpDistance \
	near/hgGoAssociation \
	near/hgKnownToSuper \
	near/hgMapToGene \
	near/hgMapViaSwissProt \
	near/hgProtIdToGenePred \
	near/hgWormLinks \
	near/pepPredToFa \
	near/hgNearTest \
	nci60 \
	orthoMap \
	overlapSelect \
	patCount \
	pslCat \
	pslCheck \
	pslCoverage \
	pslDropOverlap \
	pslFilter \
	pslFilterPrimers \
	pslGlue \
	pslHitPercent \
	pslIntronsOnly \
	pslMap \
	pslPairs \
	pslPretty \
	pslQuickFilter \
	pslReps \
	pslSelect \
	pslSimp \
	pslSort \
	pslSortAcc \
	pslSwap \
	pslUnpile \
	qaToQac \
	qacToQa \
	qacToWig \
	ratStuff \
	relPairs \
	recycleDb \
	reviewSanity \
	sage \
	scanRa \
	semiNorm \
	sim4big \
	spideyToPsl \
	splitFa \
	splitFaIntoContigs \
	test \
	trfBig \
	uniqSize \
	updateStsInfo

