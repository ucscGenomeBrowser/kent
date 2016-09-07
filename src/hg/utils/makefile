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
	bigPslToPsl \
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
	hubCheck \
	hubPublicCheck \
	makeTableList \
	oligoMatch \
	overlapSelect \
	positionalTblCheck \
	pslToBigPsl \
	pslMap \
	tdbQuery \
	transMapPslToGenePred \
	twoBitMask

SCRIPT_FILES = \
	dbNsfpToBed.pl \
	dumpDb.pl \
	mergeOverlapBed4.pl \
	mergeSortedBed3Plus.pl

# List of directories to build of interest only at UCSC
DIRS = ${buildRefresh} \
	automation \
	checkUrlsInTable \
	geoMirrorNode \
	makeTrackIndex \
	mysqlSecurityCheck \
	pslUniq \
	refSeqGet \
	safePush \
	toDev64 \
	wigBedToStep

TEST_DIRS = \
	genePredFilter \
	genePredToProt \
	gff3ToGenePred \
	gff3ToPsl \
	gtfToGenePred \
	overlapSelect \
	pslMap \
	pslToBigPsl \
	refSeqGet \
	transMapPslToGenePred

APPS = $(DIRS) $(UTILS_APPLIST)

all:: utils

utils: $(APPS:%=%.utils)
%.utils:
	cd $* && echo $* && $(MAKE)

userApps: $(UTILS_APPLIST:%=%.userApp)
%.userApp:
	cd $* && echo $* && $(MAKE)

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
