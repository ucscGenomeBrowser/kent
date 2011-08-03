# Build encode loader

include ../../../inc/common.mk

SCRIPT_FILES = \
	doEncodeLoad.pl doEncodeUnload.pl

PIPELINE_PATH=/hive/groups/encode/dcc/pipeline


all:
	@${MAKE} --no-print-directory SCRIPTS=${SCRIPTS} scripts

development:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_${USER}/bin/scripts scripts schemas

beta:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_beta/bin/scripts scripts schemas

prod:
	@${MAKE} --no-print-directory SCRIPTS=${PIPELINE_PATH}/encpipeline_prod/bin/scripts scripts schemas

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
