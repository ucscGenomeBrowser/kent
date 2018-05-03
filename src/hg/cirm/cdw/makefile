kentSrc = ../../..
# Build all subdirectories we care about

include $(kentSrc)/inc/common.mk


# List of directories to build
USE_DIRS = $(UTIL_DIRS) $(CGI_DIRS)

UTIL_DIRS = \
    cdwAddAssembly \
    cdwAddQaContamTarget \
    cdwAddQaEnrichTarget \
    cdwAddSubscriber \
    cdwBackup \
    cdwChangeFormat \
    cdwCheckDataset \
	cdwCheckValidation \
	cdwCorrectFileTags \
    cdwCreateUser \
    cdwCreateGroup \
    cdwDumpManiFiles \
    cdwFakeManifestFromSubmit \
    cdwFindSymlinkable \
    cdwGroupUser \
    cdwJob \
    cdwMakeContaminationQa \
    cdwMakeEnrichments \
    cdwMakeFileTags \
    cdwMakePairedEndQa \
    cdwMakeRepeatQa \
    cdwMakeReplicateQa \
    cdwMakeTrackViz \
    cdwMakeValidFile \
    cdwQaAgent \
    cdwQaEvaluate \
    cdwPublicSubset \
    cdwReallyRemoveFiles \
    cdwReallyRemoveDataset \
    cdwRenameFiles \
    cdwRenameDataset \
    cdwRetryJob \
    cdwRunDaemon \
    cdwRunOnIds \
    cdwSubmit \
    cdwSummary \
    cdwSwapInSymLink \
    cdwTextForIndex \
    cdwUndeprecate \
    cdwUnlockSubmittedFile \
    cdwUnlockSubmittedFiles \
    utils/cdwFastqPolyFilter \
    utils/cdwVcfStats

CGI_DIRS = \
    cdwGetFile \
    cdwServeTagStorm \
    cdwWebBrowse 

TEST_DIRS = 

utils: cdwLib
	${MAKE} buildUtils

cdwLib:
	cd lib && ${MAKE}

buildUtils: $(UTIL_DIRS:%=%.buildUtils)
%.buildUtils:
	cd $* && $(MAKE)

test::	${TEST_DIRS:%=%.test}
%.test:
	cd $* && ${MAKE} test

clean:: $(USE_DIRS:%=%.clean)
	- find . -name '*.o' -exec rm {} \;
	cd lib && ${MAKE} clean
%.clean:
	(cd $* && ${MAKE} clean)

cgi:: cdwLib tagStormCgi
	${MAKE} buildCgi

buildCgi:: $(CGI_DIRS:%=%.buildCgi)
%.buildCgi:
	cd $* && ${MAKE}

alpha:: cdwLib tagStormAlpha
	${MAKE} buildAlpha

buildAlpha:: $(CGI_DIRS:%=%.buildAlpha)
%.buildAlpha:
	(cd $* && ${MAKE} alpha)

tagStormCgi:
	cd ../../../tagStorm && ${MAKE} cgi

tagStormAlpha:
	cd ../../../tagStorm && ${MAKE} alpha

