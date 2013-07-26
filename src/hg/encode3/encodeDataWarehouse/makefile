# Build all subdirectories we care about

include ../../../inc/common.mk


# List of directories to build
USE_DIRS = $(UTIL_DIRS) $(CGI_DIRS)

UTIL_DIRS = \
    edwAddAssembly \
    edwAddQaContamTarget \
    edwAddQaEnrichTarget \
    edwAddSubscriber \
    edwChangeFormat \
    edwCorrectFileTags \
    edwCreateUser \
    edwMakeEnrichments \
    edwMakeReplicateQa \
    edwMakeValidFile \
    edwReallyRemoveFiles \
    edwRunDaemon \
    edwSubmit

CGI_DIRS = \
    edwScriptSubmit \
    edwScriptSubmitStatus \
    edwWebAuthLogin \
    edwWebAuthLogout \
    edwWebBrowse \
    edwWebCreateUser \
    edwWebDeprecate \
    edwWebRegisterScript \
    edwWebSubmit

TEST_DIRS = \
	encode2BedDoctor 

all:: edwLib utils

edwLib:
	(cd lib && make)


utils: $(USE_DIRS:%=%.utils)
%.utils:
	(cd $* && $(MAKE))

test::	${TEST_DIRS:%=%.test}
%.test:
	(cd $* && ${MAKE} test)

clean:: $(USE_DIRS:%=%.clean)
	- find . -name '*.o' -exec rm {} \;
%.clean:
	(cd $* && ${MAKE} clean)

alpha:: $(CGI_DIRS:%=%.alpha)
%.alpha:
	(cd $* && ${MAKE} alpha)

