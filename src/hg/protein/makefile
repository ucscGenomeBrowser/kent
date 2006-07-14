# Build all directories in utils

include ../../inc/common.mk

all::    utils

utils:
	@for D in $(USE_DIRS) x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && $(MAKE) 'HG_WARN=${HG_WARN_ERR}' ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done

# List of directories to build
USE_DIRS = $(DIRS)

clean: 
	@for D in ${USE_DIRS}; do \
		(cd $${D} && ${MAKE} clean;) \
	done

# no makefile for:
#       pfamXref
#	spXref2


DIRS = \
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
	pfamXref \
	rmKGPepMrna \
	spOrganism \
	spTest \
	spToDb \
	spLoadPsiBlast \
	spLoadRankProp \
	spToProteinsVar \
	spXref3 \
	spm3 \
	spm6 \
	spm7 \
	spToProteins

