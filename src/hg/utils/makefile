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
	bigMafToMaf \
	bigPslToPsl \
	binFromRange \
	chromGraphFromBin \
	chromGraphToBin \
	crTreeIndexBed \
	crTreeSearchBed \
	gapToLift \
	genePredFilter \
	genePredToBigGenePred \
	genePredToProt \
	gff3ToGenePred \
	gff3ToPsl \
	gtfToGenePred \
	hgvsToVcf \
	hubCheck \
	hubClone \
	hubPublicCheck \
	mafToBigMaf \
	makeTableList \
	oligoMatch \
	overlapSelect \
	positionalTblCheck \
	pslToBigPsl \
	pslMap \
	pslMapPostChain \
	pslRc \
	tdbQuery \
	transMapPslToGenePred \
	twoBitMask

USER_APP_SCRIPTS = \
	vai.pl

SCRIPT_FILES = \
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
	makeTrackIndex \
	mysqlSecurityCheck \
	pslFixCdsJoinGap \
	pslMismatchGapToBed \
	pslUniq \
	refSeqGet \
	safePush \
	toDev64 \
	vcfToHgvs \
	wigBedToStep

TEST_DIRS = \
	bedMergeAdjacent \
	genePredFilter \
	genePredToProt \
	gff3ToGenePred \
	gff3ToPsl \
	gtfToGenePred \
	hgvsToVcf \
	hubCheck \
	overlapSelect \
	pslMap \
	pslRc \
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
