# Build all directories in utils

# 	$Id: makefile,v 1.156 2010/03/03 02:53:30 markd Exp $

BROWSER_BINS=cartReset das hgBlat makeDb/hgCgiData hgConvert hgHubConnect \
        hgGateway hgGene hgGenome hgLogin hgPcr hgSession hgSuggest \
        hgTables hgTracks hgTrackUi hgc hgApi cartDump near/hgNear \
	hgLiftOver visiGene/hgVisiGene \
	encode/mkEncodeFrameset encode/hgEncodeDataVersions \
	encode/hgEncodeVocab encode/hgEncodeApi \
	phyloGif hgCustom hgPal hgFileUi hgFileSearch\
	makeDb/hgCgiData js htdocs/style 

ifdef LOWELAB
    BROWSER_BINS += $(HOME)/lowelab/src/browser/tooltip $(HOME)/lowelab/src/browser/chooseorg 
endif

BROWSER_LOADERS = makeDb/hgLoadBed makeDb/hgLoadWiggle makeDb/hgLoadMaf

ifneq ($(findstring $(shell hostname),hgwdev hgwbeta),)
    ENCODE_FILES =
endif


TEST_DIRS = lib autoDtd autoSql blastToPsl checkTableCoords embossToPsl \
            genePredSingleCover genePredHisto genePredToBed genePredToGtf \
	    genePredToMafFrames gbGetEntries hgGetAnn liftAcross \
            mrnaToGene pslCDnaFilter pslDiff pslHisto \
            pslPartition pslSelect pslStats pslSwap pslToBed spideyToPsl \
	    sqlToXml xmlToSql utils

BROKEN_TEST_DIRS = liftOver scaffoldFaToAgp

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

# List of directories to build for userApps
UTILS_APPLIST = \
	bedSort \
	liftUp \
	liftOver \
	bedIntersect \
	bedItemOverlapCount \
	bedToGenePred \
	blastToPsl \
	checkCoverageGaps \
	checkHgFindSpec \
	checkTableCoords \
	dbTrash \
	encode3/validateFiles \
	encode3/validateManifest \
	estOrient \
	featureBits \
	genePredHisto \
	genePredSingleCover \
	genePredToBed \
	genePredToFakePsl \
	genePredToMafFrames \
	gigAssembler/checkAgpAndFa \
	hgsql \
	hgsqldump \
	maskOutFa \
	mrnaToGene \
	ratStuff/mafsInRegion \
	ratStuff/mafSpeciesSubset \
	pslCDnaFilter \
	genePredCheck \
	genePredToGtf \
	pslCat \
	pslCheck \
	pslDropOverlap \
	pslFilter \
	pslHisto \
	pslPairs \
	pslPartition \
	pslPretty \
	pslRecalcMatch \
	pslReps \
	pslSelect \
	pslSort \
	pslStats \
	pslSwap \
	pslToBed \
	pslxToFa \
	qaToQac \
	qacAgpLift \
	qacToQa \
	qacToWig \
	sqlToXml \
	trfBig \
	xmlCat \
	xmlToSql

# List of directories to build at UCSC
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
	bedSplitOnChrom \
	borfBig \
	ctgFaToFa \
	ctgToChromFa \
	embossToPsl \
	fakeFinContigs \
	fakeOut \
	ffaToFa \
	fishClones \
	fqToQa \
	fqToQac \
	fragPart \
	gbGetEntries \
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
	hgTablesTest \
	hgChroms \
	hgGetAnn \
	hgKnownGeneList \
	hgSelect \
	hgSpeciesRna \
	hgsqlLocal \
	hgsqladmin \
	hgsqldumpLocal \
	hgsqlimport \
        hgsqlSwapTables \
        hgsqlTableDate \
	intronEnds \
	lib \
	liftAcross \
	liftAgp \
	liftFrags \
	lfsOverlap \
	makeDb \
	mdToNcbiLift \
	mouseStuff \
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
	near/knownToHprd \
	nci60 \
	orthoMap \
	patCount \
	perf \
	pslCoverage \
	pslDiff \
	pslFilterPrimers \
	pslGlue \
	pslHitPercent \
	pslIntronsOnly \
	pslQuickFilter \
	pslSimp \
	pslSortAcc \
	pslSplitOnTarget \
	pslUnpile \
	qa \
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
	test \
	txCds \
	txGene \
	txGraph \
	utils \
	uniqSize \
	updateStsInfo \
	visiGene/knownToVisiGene

APPS = $(UTIL_DIRS) $(UTILS_APPLIST)

all::    cgi utils

hgLib:
	@./sqlEnvTest.sh
	cd lib && ${MAKE}

userApps: $(UTILS_APPLIST:%=%.userApp)
%.userApp: hgLib
	cd $* && echo $* && $(MAKE)

utils: $(APPS:%=%.utils)
%.utils: hgLib
	cd $* && echo $* && $(MAKE)

# Compile only, the cgi's for those builds without write access
#	to apache cgi-bin directories  (this compiles and links executables)

compile: ${BROWSER_BINS:%=%.compile} ${BROWSER_LOADERS:%=%.compile}
%.compile: hgLib
	cd $* && echo $* && $(MAKE) compile

# Build cgi's
#   cgi: Destination for the binaries is /usr/local/apache/cgi-bin-${USER}

cgi: ${BROWSER_BINS:%=%.bins} ${BROWSER_LOADERS:%=%.cgi} ${BROWSER_LOADERS:%=%.cgi} ${ENCODE_FILES:%=%.cgi}
%.bins: hgLib
	cd $* && echo $* && $(MAKE)
%.cgi: hgLib
	cd $* && echo $* && $(MAKE) cgi

#   alpha: Destination for the binaries is /usr/local/apache/cgi-bin/

alpha:	 ${BROWSER_BINS:%=%.alpha} ${BROWSER_LOADERS:%=%.alpha} ${ENCODE_FILES:%=%.alpha}
%.alpha: hgLib
	cd $* && echo $* && $(MAKE) alpha

#   beta: Destination for the binaries is /usr/local/apache/cgi-bin-beta/

beta: ${BROWSER_BINS:%=%.beta} ${BROWSER_LOADERS:%=%.beta}
%.beta: hgLib
	cd $* && echo $* && $(MAKE) beta

# install: Destination for the binaries is ${DESTDIR}/usr/local/apache/cgi-bin

install: ${BROWSER_BINS:%=%.install} ${BROWSER_LOADERS:%=%.install}
%.install: hgLib
	cd $* && echo $* && $(MAKE) install

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

