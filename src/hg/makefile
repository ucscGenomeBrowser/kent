# Build all directories in utils

BROWSER_BINS=cartReset das hgBlat hgConvert \
        hgGateway hgGene hgPcr hgSession hgTables hgText hgTracks \
	hgTrackUi hgc cartDump near/hgNear \
	protein/pbGateway protein/pbTracks protein/pbGlobal \
	hgLiftOver hgc/cgiClustalw visiGene/hgVisiGene \
	encode/mkEncodeFrameset encode/hgEncodeDataVersions

TEST_DIRS = lib autoDtd autoSql blastToPsl checkTableCoords embossToPsl \
            genePredSingleCover genePredHisto genePredToMafFrames\
	    gbGetEntries \
            hgGetAnn mrnaToGene overlapSelect pslCDnaFilter pslDiff \
            pslHisto pslMap pslPartition pslSelect pslStats pslSwap spideyToPsl \
	    sqlToXml xmlToSql

BROKEN_TEST_DIRS = liftOver scaffoldFaToAgp


all::    cgi utils

hgLib:
	@./sqlEnvTest.sh
	cd lib && ${MAKE}

utils:	hgLib
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

compile:	hgLib
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE} compile;) \
	done

# Build cgi's
#   cgi: Destination for the binaries is /usr/local/apache/cgi-bin-${USER}

cgi:	hgLib
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE};) \
	done

#   alpha: Destination for the binaries is /usr/local/apache/cgi-bin/

alpha:	hgLib
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE} alpha;) \
	done

#   beta: Destination for the binaries is /usr/local/apache/cgi-bin-beta/

beta:	hgLib
	@for D in ${BROWSER_BINS}; do \
		(cd $${D} && ${MAKE} beta;) \
	done

# install: Destination for the binaries is ${DESTDIR}/usr/local/apache/cgi-bin

install:	hgLib
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

test::	${TEST_DIRS:%=%.test}

%.test: hgLib
	cd $* && ${MAKE} test

UTIL_DIRS = \
	affyTransciptome \
	agpCloneCheck \
	agpCloneList \
	agpAllToFaFile \
	agpToFa \
	agpToGl \
	autoDtd \
	autoSql \
	autoXml \
	bedIntersect \
	bedItemOverlapCount \
	bedSort \
	bedToGenePred \
	blastToPsl \
	borfBig \
	checkHgFindSpec \
	checkTableCoords \
	ctgFaToFa \
	ctgToChromFa \
	estOrient \
	embossToPsl \
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
	genePredHisto \
	genePredSingleCover \
	genePredToGtf \
	genePredToFakePsl \
	genePredToMafFrames \
	getFeatDna \
	getRna \
	getRnaPred \
	gigAssembler/checkAgpAndFa \
	gpStats \
	gpToGtf \
	gpcrParser \
	gsBig \
	hgTablesTest \
	hgChroms \
	hgGetAnn \
	hgSpeciesRna \
	hgsql \
	hgsqladmin \
	hgsqldump \
	hgsqlimport \
	intronEnds \
	lib \
	liftAgp \
	liftFrags \
	liftOver \
	liftUp \
	lfsOverlap \
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
	near/hgNetDist \
	nci60 \
	orthoMap \
	overlapSelect \
	patCount \
	protein \
	pslCDnaFilter \
	pslCat \
	pslCheck \
	pslCoverage \
	pslDiff \
	pslDropOverlap \
	pslFilter \
	pslFilterPrimers \
	pslGlue \
	pslHisto \
	pslHitPercent \
	pslIntronsOnly \
	pslMap \
	pslPartition \
	pslPairs \
	pslPretty \
	pslQuickFilter \
	pslRecalcMatch \
	pslReps \
	pslSelect \
	pslSimp \
	pslSort \
	pslSortAcc \
	pslStats \
	pslSwap \
	pslToBed \
	pslUnpile \
	pslxToFa \
	qa \
	qaToQac \
	qacAgpLift \
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
	snpException \
	snp/snpMask \
	snp/snpValid \
	spideyToPsl \
	splitFa \
	splitFaIntoContigs \
	sqlToXml \
	test \
	trfBig \
	uniqSize \
	updateStsInfo \
	xmlCat \
	xmlToSql

