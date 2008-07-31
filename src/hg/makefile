# Build all directories in utils

# 	$Id: makefile,v 1.143.10.1 2008/07/31 02:23:57 markd Exp $

BROWSER_BINS=cartReset das hgBlat makeDb/hgCgiData hgConvert \
        hgGateway hgGene hgGenome hgPcr hgSession hgTables hgTracks \
	hgTrackUi hgc cartDump near/hgNear \
	protein/pbGateway protein/pbTracks protein/pbGlobal \
	hgLiftOver visiGene/hgVisiGene \
	encode/mkEncodeFrameset encode/hgEncodeDataVersions \
	phyloGif hgCustom \
	makeDb/hgCgiData js

BROWSER_LOADERS = makeDb/hgLoadBed makeDb/hgLoadWiggle makeDb/hgLoadMaf

TEST_DIRS = lib autoDtd autoSql blastToPsl checkTableCoords embossToPsl \
            genePredSingleCover genePredHisto genePredToGtf \
	    genePredToMafFrames gbGetEntries hgGetAnn liftAcross \
            mrnaToGene overlapSelect pslCDnaFilter pslDiff pslHisto \
            pslMap pslPartition pslSelect pslStats pslSwap spideyToPsl \
	    sqlToXml xmlToSql

BROKEN_TEST_DIRS = liftOver scaffoldFaToAgp


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

UTIL_DIRS = \
	affyTransciptome \
	agpCloneCheck \
	agpCloneList \
	agpAllToFaFile \
	agpToFa \
	agpToGl \
	altSplice \
	autoDtd \
	autoSql \
	autoXml \
	bedOrBlocks \
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
	dbTrash \
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
	hgKnownGeneList \
	hgSelect \
	hgSpeciesRna \
	hgsql \
	hgsqlLocal \
	hgsqladmin \
	hgsqldump \
	hgsqldumpLocal \
	hgsqlimport \
	intronEnds \
	lib \
	liftAcross \
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
	perf \
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
	pslSplitOnTarget \
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
	rnaStructure \
	sage \
	scanRa \
	semiNorm \
	sim4big \
	snpException \
	snp \
	spideyToPsl \
	splitFa \
	splitFaIntoContigs \
	sqlToXml \
	test \
	trfBig \
	txCds \
	txGene \
	txGraph \
	utils \
	uniqSize \
	updateStsInfo \
	visiGene/knownToVisiGene \
	xmlCat \
	xmlToSql


all::    cgi utils

hgLib:
	@./sqlEnvTest.sh
	cd lib && ${MAKE}

utils: $(USE_DIRS:%=%.utils)
%.utils: hgLib
	( cd $* && echo $* && $(MAKE) )

# Compile only, the cgi's for those builds without write access
#	to apache cgi-bin directories  (this compiles and links executables)

compile: ${BROWSER_BINS:%=%.compile} ${BROWSER_LOADERS:%=%.compile}
%.compile: hgLib
	( cd $* && echo $* && $(MAKE) compile)

# Build cgi's
#   cgi: Destination for the binaries is /usr/local/apache/cgi-bin-${USER}

cgi: ${BROWSER_BINS:%=%.bins} ${BROWSER_LOADERS:%=%.cgi}
%.bins: hgLib
	( cd $* && echo $* && $(MAKE) )
%.cgi: hgLib
	( cd $* && echo $* && $(MAKE) cgi)

#   alpha: Destination for the binaries is /usr/local/apache/cgi-bin/

alpha:	 ${BROWSER_BINS:%=%.alpha} ${BROWSER_LOADERS:%=%.alpha}
%.alpha: hgLib
	( cd $* && echo $* && $(MAKE) alpha)

#   beta: Destination for the binaries is /usr/local/apache/cgi-bin-beta/

beta: ${BROWSER_BINS:%=%.beta} ${BROWSER_LOADERS:%=%.beta}
%.beta: hgLib
	( cd $* && echo $* && $(MAKE) beta)

# install: Destination for the binaries is ${DESTDIR}/usr/local/apache/cgi-bin

install: ${BROWSER_BINS:%=%.install} ${BROWSER_LOADERS:%=%.install}
%.install: hgLib
	( cd $* && echo $* && $(MAKE) install)

clean::
	cd lib && ${MAKE} clean
	@for D in ${BROWSER_BINS} ${BROWSER_LOADERS}; do \
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

