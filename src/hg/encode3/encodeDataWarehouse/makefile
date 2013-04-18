# Build all subdirectories we care about

include ../../../inc/common.mk


# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
    lib \
    edwAddAssembly \
    edwAddQaEnrichTarget \
    edwAddSubscriber \
    edwCreateUser \
    edwMakeEnrichments \
    edwMakeReplicateQa \
    edwMakeValidFile \
    edwResetPassword \
    edwSubmit \
    edwSubmitSpooler \
    edwWebSubmit

TEST_DIRS = \
	encode2BedDoctor 

all:: utils

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

