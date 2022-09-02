# Build all directories in utils
# test comment
# 	$Id: makefile,v 1.21 2010/06/04 00:12:42 larrym Exp $

kentSrc = ../..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

# to check for Mac OSX Darwin specifics:
UNAME_S := $(shell uname -s)

# can not build this on Mac OSX - has functions not supported
ifeq ($(UNAME_S),Darwin)
  buildRefresh=
else
  buildRefresh=refreshNamedSessionCustomTracks
endif

# List of directories to build for userApps
UTILS_APPLIST = \
	bedExtendRanges \
	bedToPsl \
	bedWeedOverlapping \
	bedMergeAdjacent \
	bedPartition \
	bigGenePredToGenePred \
	bigMafToMaf \
	bigPslToPsl \
	binFromRange \
	chromGraphFromBin \
	chromGraphToBin \
	crTreeIndexBed \
	crTreeSearchBed \
	faToVcf \
	gapToLift \
	genePredFilter \
	genePredToBigGenePred \
	genePredToProt \
	gencodeVersionForGenes \
	gff3ToGenePred \
	gff3ToPsl \
	gtfToGenePred \
	hicInfo \
	hgvsToVcf \
	hubCheck \
	hubClone \
	hubPublicCheck \
	mafToBigMaf \
	makeTableList \
	oligoMatch \
	overlapSelect \
	positionalTblCheck \
	pslLiftSubrangeBlat \
	pslToBigPsl \
	raSqlQuery \
	rmskAlignToPsl \
	tdbQuery \
	transMapPslToGenePred \
	twoBitMask \
	vcfToBed

USER_APP_SCRIPTS = \
	vai.pl

SCRIPT_FILES = \
	archiveTracks.sh \
	dbNsfpToBed.pl \
	dumpDb.pl \
	mergeOverlapBed4.pl \
	mergeSortedBed3Plus.pl \
	mysqlRowSizes.pl \
	${USER_APP_SCRIPTS}

APP_SCRIPTS = ${SCRIPT_FILES}

# List of directories to build of interest only at UCSC
DIRS = ${buildRefresh} \
	automation \
	blatServersCheck \
	buildReleaseLog \
	checkUrlsInTable \
	geoMirrorNode \
	hubCrawl \
	makeTrackIndex \
	mysqlSecurityCheck \
	phyloRenameAndPrune \
	pslFixCdsJoinGap \
	pslMismatchGapToBed \
	pslUniq \
	refSeqGet \
	safePush \
	scaffoldFaToAgp \
	toDev64 \
	vcfRenameAndPrune \
	vcfToHgvs \
	wigBedToStep

TEST_DIRS = \
	bedMergeAdjacent \
	bedPartition \
	genePredFilter \
	genePredToProt \
	gff3ToGenePred \
	gff3ToPsl \
	gtfToGenePred \
	hgvsToVcf \
	hubCheck \
	overlapSelect \
	pslToBigPsl \
	refSeqGet \
	transMapPslToGenePred \
	vcfToHgvs

APPS = $(DIRS) $(UTILS_APPLIST)

all:: utils

utils: $(APPS:%=%.utils) $(APP_SCRIPTS:%=%.appScript)
%.utils:
	cd $* && echo $* && $(MAKE)
%.appScript:
	chmod +x $*
	rm -f ${DESTDIR}${SCRIPTS}/$*
	cp -p $* ${DESTDIR}${SCRIPTS}/$*

userApps: $(UTILS_APPLIST:%=%.userApp) $(USER_APP_SCRIPTS:%=%.userAppScript)
%.userApp:
	cd $* && echo $* && $(MAKE)
%.userAppScript:
	chmod +x $*
	rm -f ${DESTDIR}${BINDIR}/$*
	cp -p $* ${DESTDIR}${BINDIR}/$*

scripts: $(SCRIPT_FILES:%=%.scripts)
%.scripts:
	chmod +x $*
	rm -f ${SCRIPTS}/$*
	cp -p $* ${SCRIPTS}/$*

alpha: scripts all

test::	${TEST_DIRS:%=%.test}
%.test:
	(cd $* && ${MAKE} test)

clean:: $(APPS:%=%.clean)
	- find . -name '*.o' -exec rm {} \;
%.clean:
	(cd $* && ${MAKE} clean)

compile:: $(APPS:%=%.compile)
	- find . -name '*.o' -exec rm {} \;
%.compile:
	(cd $* && ${MAKE} compile)
