kentSrc = ../../../..
# Build all subdirectories we care about

include ../../../../inc/common.mk

UTIL_DIRS = \
    edwComparePeaks \
    edwReplicatedPeaks \
    edwSamPairedEndStats \
    edwSolexaToSangerFastq 

ifeq (${USE_BAM},1)
  UTIL_DIRS += edwBamFilter \
    edwBamRemoveChrom \
    edwBamStats \
    edwSamRepeatAnalysis
endif

buildUtils: $(UTIL_DIRS:%=%.buildUtils)
%.buildUtils:
	cd $* && $(MAKE)


