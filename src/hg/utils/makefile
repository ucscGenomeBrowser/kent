# Build all directories in utils
# test comment
# 	$Id: makefile,v 1.21 2010/06/04 00:12:42 larrym Exp $

include ../../inc/common.mk

# List of directories to build for userApps
UTILS_APPLIST = \
	bedExtendRanges \
	bedToPsl \
	bedWeedOverlapping \
	chromGraphFromBin \
	chromGraphToBin \
	crTreeIndexBed \
	crTreeSearchBed \
	gapToLift \
	gff3ToGenePred \
	gff3ToPsl \
	gtfToGenePred \
	hubCheck \
	makeTableList \
	oligoMatch \
	overlapSelect \
	positionalTblCheck \
	pslMap \
	tdbQuery \
	twoBitMask

SCRIPT_FILES = \
	dbNsfpToBed.pl \
	dumpDb.pl \
	mergeOverlapBed4.pl \
	mergeSortedBed3Plus.pl

# List of directories to build of interest only at UCSC
DIRS = \
	hubPublicCheck \
	safePush \
	toDev64 \
	automation \
	refreshNamedSessionCustomTracks \
	refSeqGet \
	wigBedToStep \
	pslUniq \
	makeTrackIndex \
	geoMirrorNode

TEST_DIRS = \
	gff3ToGenePred \
	gff3ToPsl \
	gtfToGenePred \
	overlapSelect \
	pslMap \
	refSeqGet

APPS = $(DIRS) $(UTILS_APPLIST)

all:: utils

utils: $(APPS:%=%.utils)
%.utils:
	(cd $* && $(MAKE))

userApps: $(UTILS_APPLIST:%=%.userApp)
%.userApp:
	( cd $* && echo $* && $(MAKE) )

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
