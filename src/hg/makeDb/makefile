# Build all directories in makeDb 

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	hgAar \
	hgAddLiftOverChain \
	hgAvidShortBed \
	hgClonePos \
	hgCountAlign \
	hgCtgPos \
	hgDeleteChrom \
	hgExonerate \
	hgExperiment \
	hgExtFileCheck \
	hgFakeAgp \
	hgFiberglass \
	hgFindSpec \
	hgFlyBase \
	hgGcPercent \
	hgGeneBands \
	hgGnfMicroarray \
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
	hgPhMouse \
	hgRatioMicroarray \
	hgRnaGenes \
	hgSanger20 \
	hgSanger22 \
	hgSoftberryHom \
	hgSoftPromoter \
	hgStanfordMicroarray \
	hgStsAlias \
	hgStsMarkers \
	hgTomRough \
	hgTpf \
	hgTraceInfo \
	hgTrackDb \
	hgWaba \
	ldHgGene \
	sanger22gtf \
	tailLines \
	vegaBuildInfo \
	hgBioCyc \
	hgKegg \
	hgKegg2 \
	hgMrnaRefseq \
	hgCGAP \
	schema \
	tfbsConsLoc \
	tfbsConsSort
