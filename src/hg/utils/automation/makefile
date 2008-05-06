# Build all directories in utils

include ../../../inc/common.mk

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
	    rm -f ${SCRIPTS}/$${F}; \
	    echo cp -p $$F ${SCRIPTS}; \
	    cp -p $$F ${SCRIPTS}; \
	 done

alpha: scripts

SCRIPT_FILES = \
	blastz-make-joblist \
	blastz-normalizeLav \
	blastz-run-ucsc \
	doBlastzChainNet.pl \
	doEnsGeneUpdate.pl \
	doHgNearBlastp.pl \
	synBlastp.csh \
	doRecipBest.pl \
	doRepeatMasker.pl \
	doWindowMasker.pl \
	doSimpleRepeat.pl \
	doSameSpeciesLiftOver.pl \
	ensVersions \
	extractNestedRepeats.pl \
	EnsGeneAutomate.pm \
	HgAutomate.pm \
	HgRemoteScript.pm \
	HgStepManager.pm \
	liftRMAlign.pl \
	makeDownloads.pl \
	makeGenomeDb.pl \
	makePushQSql.pl \
	monthlyUpdateIgtc.csh \
	simplePartition.pl \
	partitionSequence.pl \
	updateIgtc.pl

