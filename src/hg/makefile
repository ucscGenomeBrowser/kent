# Build all directories in utils

kentSrc = ..
include ${kentSrc}/inc/localEnvironment.mk

BROWSER_BINS=cartReset das hgBlat makeDb/hgCgiData hgCollection hgConvert \
	hgHubConnect hgGateway hgGene hgGenome hgIntegrator hgLogin hgPcr \
	hgSession hgSuggest hgTables hgTracks hgTrackUi hgUserSuggestion \
	hgc hgApi cartDump near/hgNear hgLiftOver hgLinkIn hgVai \
	visiGene/hgVisiGene hgMenubar hgPublicSessions \
	encode/hgEncodeDataVersions encode/hgEncodeVocab encode/hgEncodeApi \
	phyloPng hgCustom hgPal hgFileUi hgFileSearch makeDb/hgCgiData \
	js htdocs/style hgMirror hgBeacon hgGtexTrackSettings pyLib \
	hgGeneGraph hgs hubApi hgPhyloPlace gar hgSearch

ifdef LOWELAB
    BROWSER_BINS += $(HOME)/lowelab/src/browser/tooltip $(HOME)/lowelab/src/browser/chooseorg
endif

BROWSER_LOADERS = makeDb/hgLoadBed makeDb/hgLoadWiggle makeDb/hgLoadMaf utils/pslToBigPsl utils/hubCheck

ifneq ($(findstring $(shell hostname),hgwdev hgwbeta),)
    ENCODE_FILES = encode3/encodeDataWarehouse
endif

# these should not be included in UTILS_APPLIST or parallel test runs will fail
TEST_DIRS = lib

TEST_EXTRA = cgilib

BROKEN_TEST_DIRS =  scaffoldFaToAgp

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

# can not build this on Mac OSX - has functions not supported
UNAME_S := $(shell uname -s) 
ifeq ($(UNAME_S),Darwin)
  buildRefresh=
else
  buildRefresh=encode3
endif

# in the kent-core version of the tree, there is no encode3 directory
ifeq ($(wildcard encode3/*),)
  buildRefresh=
endif

# List of directories to build for userApps
UTILS_APPLIST = \
	autoDtd \
	autoSql \
	autoXml \
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
	geneBounds/clusterGenes \
	genePredHisto \
	genePredSingleCover \
	genePredToBed \
	genePredToFakePsl \
	genePredToMafFrames \
	getRna \
	getRnaPred \
	gigAssembler/checkAgpAndFa \
	hgSpeciesRna \
	hgsql \
	hgsqldump \
	maskOutFa \
	mrnaToGene \
	ratStuff/mafsInRegion \
	ratStuff/mafSpeciesSubset \
	ratStuff/mafToSnpBed \
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
	pslSomeRecords \
	pslSort \
	pslSortAcc \
	pslSplitOnTarget \
	pslStats \
	pslToBed \
	pslToChain \
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
	agpMergeChromScaf \
	agpToFa \
	agpToGl \
	altSplice \
	bedOrBlocks \
	bedSplitOnChrom \
	borfBig \
	ctgFaToFa \
	ctgToChromFa \
	embossToPsl \
	${buildRefresh} \
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
	gpStats \
	gpToGtf \
	gpcrParser \
	gsBig \
	hgTablesTest \
	hgChroms \
	hgGetAnn \
	hgKnownGeneList \
	hgSelect \
	hgsqladmin \
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

all::    cgi .WAIT utils

hgLib:
	cd lib && ${MAKE}
ifneq ($(wildcard cgilib/makefile),)
	cd cgilib && ${MAKE}
endif

userApps: $(UTILS_APPLIST:%=%.userApp)
	cd altSplice/lib && $(MAKE)
	cd altSplice/altSplice && $(MAKE) bedToExons
	cd mouseStuff && $(MAKE) userApps
	cd ratStuff && $(MAKE) userApps

# tolerate missing directories, as some were removed in kent-core
%.userApp: hgLib
	if test -d $* ; then cd $* && echo $* && $(MAKE) ; fi

utils: $(APPS:%=%.utils)
# tolerate missing directories, as some were removed in kent-core
%.utils: hgLib
	if test -d $* ; then cd $* && echo $* && $(MAKE) ; fi

# Compile only, the cgi's for those builds without write access
#	to apache cgi-bin directories  (this compiles and links executables)

compile: ${BROWSER_BINS:%=%.compile} ${BROWSER_LOADERS:%=%.compile}
%.compile: hgLib
	cd $* && echo $* && $(MAKE) compile

# Build cgi's
#   cgi: Destination for the binaries is /usr/local/apache/cgi-bin-${USER}

cgi: ${BROWSER_BINS:%=%.bins} ${BROWSER_LOADERS:%=%.cgi} ${ENCODE_FILES:%=%.cgi}
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

clean_browser:: ${BROWSER_BINS:%=%_clean_browser} ${BROWSER_LOADERS:%=%_clean_browser}
	cd lib && ${MAKE} clean
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm

%_clean_browser:
	${MAKE} -C $* clean

clean_utils:: ${UTIL_DIRS:%=%_clean_utils}
%_clean_utils:
	if test -s $* ; then ${MAKE} -C $* clean ; fi

clean:: clean_browser clean_utils ${TEST_DIRS:%=%.clean} ${TEST_EXTRA:%=%.cleanExtra}

%.clean:
	${MAKE} -C $* clean

# these directories are absent in the kent-core repo due to license issues
%.cleanExtra:
	@test -s $*/makefile && (${MAKE} -C $* clean) || true


# tests tests all use test.chromInfo
.NOTPARALLEL: checkTableCoords.testAll checkTableCoords.test vcfToHgvs.testAll vcfToHgvs.test


testAll: $(APPS:%=%.testAll)

%.testAll:
	@test -s $*/tests/makefile && (echo $*; cd $* && $(MAKE) test) || true

test::	${TEST_DIRS:%=%.test} ${TEST_EXTRA:%=%.test} testAll

%.test: hgLib
	${MAKE} -C $* test

