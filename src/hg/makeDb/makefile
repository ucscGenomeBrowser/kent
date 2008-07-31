# Build all directories in makeDb 

include ../../inc/common.mk

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	hgAar \
	hgAddLiftOverChain \
	hgClonePos \
	hgCountAlign \
	hgCtgPos \
	hgDeleteChrom \
	hgExperiment \
	hgExtFileCheck \
	hgFakeAgp \
	hgFindSpec \
	hgGcPercent \
	hgGeneBands \
	hgGenericMicroarray \
	hgGoldGapGl \
	hgKgGetText \
	hgKgMrna \
	hgKnownMore \
	hgKnownMore.oo21 \
	hgLoadBed \
	hgLoadBlastTab \
	hgLoadChain \
	hgLoadChromGraph \
	hgLoadGenePred \
	hgLoadItemAttr \
	hgLoadMaf \
	hgLoadMafFrames \
	hgLoadNet \
	hgLoadOut \
	hgLoadPsl \
	hgLoadSeq \
	hgLoadSample \
	hgLoadSqlTab \
	hgLoadWiggle \
	hgMapMicroarray \
	hgMedianMicroarray \
	hgNibSeq \
	hgPepPred \
	hgRatioMicroarray \
        hgDropSplitTable \
        hgRenameSplitTable \
	hgSanger20 \
	hgSanger22 \
	hgStanfordMicroarray \
	hgStsAlias \
	hgStsMarkers \
	hgTomRough \
	hgTpf \
	hgTraceInfo \
	hgTrackDb \
	hgWaba \
	ldHgGene \
	hgMrnaRefseq \
	schema \
	tfbsConsLoc \
	tfbsConsSort \
	outside \
	genbank

all: $(USE_DIRS:%=%.all)
%.all:
	( cd $* && echo $* && $(MAKE) )
