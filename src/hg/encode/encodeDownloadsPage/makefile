# Build encode loader

include ../../../inc/common.mk

SCRIPT_FILES = \
	encodeDownloadsPage.pl encodeDownloadsPage.pl

all:
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts scripts

alpha: scripts

install:
	@${MAKE} --no-print-directory SCRIPTS=${ENCODE_PIPELINE_BIN} scripts

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
            perl -c $$F && cp -upv $$F ${SCRIPTS}; \
	done

test:
	encodeDownloadsPage.pl test.html /usr/local/apache/htdocs/goldenPath/hg18/wgEncodeYaleChIPseq
