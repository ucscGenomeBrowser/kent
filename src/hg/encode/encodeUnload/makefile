include ../../../inc/common.mk

SCRIPT_FILES = \
	doEncodeUnload.rb

all:
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts scripts

alpha: scripts

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
            ruby -c $$F && cp -upv $$F ${SCRIPTS}; \
	done

test:
	doEncodeUnload.rb load /cluster/store3/encpipeline_dev/43

