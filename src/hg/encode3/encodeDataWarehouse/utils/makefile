# Build all subdirectories we care about

include ../../../../inc/common.mk

UTIL_DIRS = \
    edwBamRemoveChrom \
    edwBamStats \
    edwBamToWig \
    edwComparePeaks \
    edwReplicatedPeaks \
    edwSamPairedEndStats \
    edwSamRepeatAnalysis \
    edwSolexaToSangerFastq \
    edwValidKey

buildUtils: $(UTIL_DIRS:%=%.buildUtils)
%.buildUtils:
	cd $* && $(MAKE)


