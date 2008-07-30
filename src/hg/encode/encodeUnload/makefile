include ../../../inc/common.mk

SCRIPT_FILES = \
	doEncodeUnload.pl

all:
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts scripts

alpha: scripts

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
            ruby -c $$F && cp -upv $$F ${SCRIPTS}; \
	done

test:
	doEncodeUnload.pl x /cluster/data/encode/pipeline/encpipeline_beta/43
