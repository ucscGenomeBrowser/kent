kentSrc = ../../..
# Build all subdirectories we care about

include ../../../inc/common.mk


# List of directories to build
USE_DIRS = $(UTIL_DIRS) 

UTIL_DIRS = \
    eapAddSoftware \
    eapAddStep \
    eapDaemon \
    eapFinish \
    eapMonitor \
    eapSchedule \
    eapUpdateSoftware

# edwFinishAnalysis depends on -luuid, which is not available on Macs without special port install.
# For now, skip it if we're on Mac.
ifneq ($(UNAME_S),Darwin)
    UTIL_DIRS += eapFinish
endif

buildUtils: edwLib $(UTIL_DIRS:%=%.buildUtils)
%.buildUtils:
	cd $* && $(MAKE)

edwLib:
	cd lib && ${MAKE}


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
