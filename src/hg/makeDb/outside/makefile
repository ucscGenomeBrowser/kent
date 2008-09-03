# Build all directories in makeDb 

include ../../../inc/common.mk

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

all: $(USE_DIRS:%=%.all)
%.all:
	( cd $* && echo $* && $(MAKE) )
