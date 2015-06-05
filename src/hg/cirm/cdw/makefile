# Build all subdirectories we care about

include ../../../inc/common.mk


# List of directories to build
USE_DIRS = $(UTIL_DIRS) $(CGI_DIRS)

UTIL_DIRS = \
    cdwAddAssembly \
    cdwAddQaContamTarget \
    cdwAddQaEnrichTarget \
    cdwAddSubscriber \
    cdwChangeFormat \
    cdwCorrectFileTags \
    cdwCreateUser \
    cdwFakeManifestFromSubmit \
    cdwJob \
    cdwMakeContaminationQa \
    cdwMakeEnrichments \
    cdwMakeFileTags \
    cdwMakePairedEndQa \
    cdwMakeRepeatQa \
    cdwMakeReplicateQa \
    cdwMakeTrackViz \
    cdwMakeValidFile \
    cdwQaEvaluate \
    cdwReallyRemoveFiles \
    cdwRetryJob \
    cdwRunDaemon \
    cdwRunOnIds \
    cdwSubmit \
    cdwSwapInSymLink \
    cdwUndeprecate

CGI_DIRS = \
    cdwWebAuthLogin \
    cdwWebAuthLogout \
    cdwWebBrowse \
    cdwWebCreateUser \
    cdwWebDeprecate \
    cdwWebSubmit \

TEST_DIRS = \
	encode2BedDoctor 

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

cgi:: cdwLib
	${MAKE} buildCgi

buildCgi:: $(CGI_DIRS:%=%.buildCgi)
%.buildCgi:
	cd $* && ${MAKE}

alpha:: cdwLib
	${MAKE} buildAlpha

buildAlpha:: $(CGI_DIRS:%=%.buildAlpha)
%.buildAlpha:
	(cd $* && ${MAKE} alpha)
