# Build all directories in utils

include ../../../inc/common.mk

PIPE_LIB = \
	Encode.pm \
	HgAutomate.pm \
	HgDb.pm \
	RAFile.pm \
	SafePipe.pm

LIB_FILES = \
	${PIPE_LIB} \
	EnsGeneAutomate.pm \
	HgRemoteScript.pm \
	HgStepManager.pm \

SCRIPT_FILES = \
	blastz-make-joblist \
	blastz-normalizeLav \
	blastz-run-ucsc \
	doBlastzChainNet.pl \
	doEnsGeneUpdate.pl \
	doHgNearBlastp.pl \
	ensemblInfo.pl \
	synBlastp.csh \
	doRecipBest.pl \
	doRepeatMasker.pl \
	doWindowMasker.pl \
	doSimpleRepeat.pl \
	doSameSpeciesLiftOver.pl \
	ensVersions \
	extractNestedRepeats.pl \
	extractGtf.pl \
	liftRMAlign.pl \
	makeDownloads.pl \
	makeGenomeDb.pl \
	makePushQSql.pl \
	monthlyUpdateIgtc.csh \
	simplePartition.pl \
	partitionSequence.pl \
	updateIgtc.pl \
	unigene.csh

all: mkDir libFiles $(SCRIPT_FILES:%=%.all)

%.all:
	rm -f ${SCRIPTS}/$*
	echo cp -p $* ${SCRIPTS}
	cp -p $* ${SCRIPTS}
	chmod +x ${SCRIPTS}/$*

mkDir:
	${MKDIR} ${SCRIPTS}
	
libFiles: mkDir $(LIB_FILES:%=%.libFile)

%.libFile:
	rm -f ${SCRIPTS}/$*
	echo cp -p $* ${SCRIPTS}
	cp -p $* ${SCRIPTS}
	chmod 664 ${SCRIPTS}/$*

alpha: all
 
development:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_${USER}/bin/scripts pipelibs

beta:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_beta/bin/scripts pipelibs 

prod:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_prod/bin/scripts pipelibs 

pipelibs: ${PIPE_LIB:%=${SCRIPTS}/%}

${SCRIPTS}/% : %
	perl -c $< 2>&1
	cp -p $< $@.tmp
	mv -f $@.tmp $@
clean:
	@echo nothing to clean in src/hg/utils/automation/
