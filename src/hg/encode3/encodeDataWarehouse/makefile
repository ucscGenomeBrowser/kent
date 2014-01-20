# Build all subdirectories we care about

include ../../../inc/common.mk


# List of directories to build
USE_DIRS = $(UTIL_DIRS) $(CGI_DIRS)

UTIL_DIRS = \
    edwAddAssembly \
    edwAddQaContamTarget \
    edwAddQaEnrichTarget \
    edwAddSubscriber \
    edwAnalysisAddJson \
    edwChangeFormat \
    edwCorrectFileTags \
    edwCreateUser \
    edwFakeManifestFromSubmit \
    edwJob \
    edwMakeContaminationQa \
    edwMakeEnrichments \
    edwMakePairedEndQa \
    edwMakeRepeatQa \
    edwMakeReplicateQa \
    edwMakeValidFile \
    edwQaEvaluate \
    edwReallyRemoveFiles \
    edwRetryJob \
    edwRunDaemon \
    edwRunOnIds \
    edwSubmit

# edwFinishAnalysis depends on -luuid, which is not available on Macs without special port install.
# For now, skip it if we're on Mac.
ifneq ($(UNAME_S),Darwin)
    UTIL_DIRS += edwFinishAnalysis
endif

CGI_DIRS = \
    edwScriptSubmit \
    edwScriptSubmitStatus \
    edwWebAuthLogin \
    edwWebAuthLogout \
    edwWebBrowse \
    edwWebCreateUser \
    edwWebDeprecate \
    edwWebRegisterScript \
    edwWebSubmit \
    edwWebXSendFile

TEST_DIRS = \
	encode2BedDoctor 

utils: edwLib
	${MAKE} buildUtils

edwLib:
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

cgi:: edwLib
	${MAKE} buildCgi

buildCgi:: $(CGI_DIRS:%=%.buildCgi)
%.buildCgi:
	cd $* && ${MAKE}

alpha:: edwLib
	${MAKE} buildAlpha

buildAlpha:: $(CGI_DIRS:%=%.buildAlpha)
%.buildAlpha:
	(cd $* && ${MAKE} alpha)
