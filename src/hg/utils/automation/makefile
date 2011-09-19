# Build all directories in utils

include ../../../inc/common.mk

LIB_FILES = \
	Encode.pm \
	EnsGeneAutomate.pm \
	HgAutomate.pm \
	HgDb.pm \
	HgRemoteScript.pm \
	HgStepManager.pm \
	RAFile.pm \
	SafePipe.pm

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

clean:
	@echo nothing to clean in src/hg/utils/automation/
