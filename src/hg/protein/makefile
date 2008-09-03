# Build all directories in utils

include ../../inc/common.mk

DIRS = \
	blastRecipBest \
	gadPos \
	hmmPfamToTab \
	kgAliasM \
	kgAliasP \
	kgAliasKgXref \
	kgAliasRefseq \
	kgProtAlias \
	kgProtAliasNCBI \
	kgBestMrna2 \
	kgBestRef2 \
	kgCheck \
	kgGetCds \
	kgGetPep \
	kgPepMrna \
	kgPick \
	kgPutBack \
	kgXref \
	kgXref2 \
	pbCalDist \
	pbCalDistGlobal \
	pbCalPi \
	pbCalResStd \
	pbHgnc \
	pfamXref \
	rmKGPepMrna \
	scopCollapse \
	spOrganism \
	spTest \
	spToDb \
	spLoadPsiBlast \
	spLoadRankProp \
	spToProteinsVar \
	spToSpXref2 \
	spXref3 \
	spm3 \
	spm6 \
	spm7 \
	spToProteins
USE_DIRS = $(DIRS)

all::    utils

utils: $(USE_DIRS:%=%.utils)

%.utils:
	(cd $* && echo $* && $(MAKE))

# List of directories to build

clean: $(USE_DIRS:%=%.clean)

%.clean:
	(cd $* && $(MAKE) clean)

# no makefile for:
#       pfamXref
#	spXref2


