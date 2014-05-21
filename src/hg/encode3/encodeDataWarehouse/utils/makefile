# Build all subdirectories we care about

include ../../../../inc/common.mk

UTIL_DIRS = \
    edwBamFilter \
    edwBamRemoveChrom \
    edwBamStats \
    edwComparePeaks \
    edwReplicatedPeaks \
    edwSamPairedEndStats \
    edwSamRepeatAnalysis \
    edwSolexaToSangerFastq \
    edwValidKey

buildUtils: $(UTIL_DIRS:%=%.buildUtils)
%.buildUtils:
	cd $* && $(MAKE)


