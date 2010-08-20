# Build encode loader

include ../../../inc/common.mk

SCRIPT_FILES = \
	doEncodeLoad.pl doEncodeUnload.pl

all:
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts scripts

alpha: scripts

install: 
	@${MAKE} --no-print-directory SCRIPTS=${ENCODE_PIPELINE_BIN} scripts schemas

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
            perl -c $$F && cp $$F ${SCRIPTS}; \
	done

schemas: 
	./installSqlSchemas.pl

test:
	doEncodeLoad.pl load /cluster/store3/encpipeline_dev/43

clean::
	echo nothing to clean in hg/encode/encodeLoad/
