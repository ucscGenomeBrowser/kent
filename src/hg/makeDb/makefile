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
	hgAvidShortBed \
	hgClonePos \
	hgCountAlign \
	hgCtgPos \
	hgFiberglass \
	hgGcPercent \
	hgGeneBands \
	hgGoldGapGl \
	hgKnownMore \
	hgKnownMore.oo21 \
	hgLoadBed \
	hgLoadChain \
	hgLoadMaf \
	hgLoadNet \
	hgLoadOut \
	hgLoadPsl \
	hgLoadRna \
	hgLoadSample \
	hgNibSeq \
	hgPepPred \
	hgPhMouse \
	hgRefSeqMrna \
	hgRefSeqStatus \
	hgRnaGenes \
	hgSanger20 \
	hgSanger22 \
	hgSoftberryHom \
	hgSoftPromoter \
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
	toBb 
