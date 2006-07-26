# Build all directories in makeDb 

include ../../../inc/common.mk

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) 'HG_WARN=${HG_WARN_ERR}' ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	allenBrain/allenCollectSeq \
	exonerateGffDoctor \
	hgAvidShortBed \
	hgBioCyc \
	hgCGAP \
	hgExonerate \
	hgFiberglass \
	hgFlyBase \
	hgGnfMicroarray \
	hgJaxQtl \
	hgKegg \
	hgKegg2 \
	hgKegg3 \
	hgLoadEranModules \
	hgPhMouse \
	hgRnaGenes \
	hgSoftPromoter \
	hgSoftberryHom \
	hgSuperfam \
	nibb/nibbImageProbes \
	nibb/nibbNameFix \
	nibb/nibbParseImageDir \
	nibb/nibbPrepImages \
	samHit \
	sanger22gtf \
	vegaBuildInfo \
	yeast/checkSgdSync \
	yeast/fixHarbisonMotifs \
	yeast/hgSgdGff3 \
	yeast/hgSgdGfp \
	yeast/hgSgdPep \
	yeast/hgYeastRegCode
