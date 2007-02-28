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
	doHgNearBlastp.pl \
	synBlastp.csh \
	doRecipBest.pl \
	doRepeatMasker.pl \
	doSameSpeciesLiftOver.pl \
	HgAutomate.pm \
	HgRemoteScript.pm \
	HgStepManager.pm \
	makeDownloads.pl \
	makeGenomeDb.pl \
	makePushQSql.pl \
	monthlyUpdateIgtc.csh \
	simplePartition.pl \
	partitionSequence.pl \
	updateIgtc.pl

