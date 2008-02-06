# Build encode validator

include ../../../inc/common.mk

SCRIPT_FILES = \
	doEncodeValidate.pl

PIPELINE_PATH=/cluster/data/encode/pipeline
PIPELINE_DIR=encpipeline_beta

alpha:  
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts scripts
	@${MAKE} --no-print-directory PIPELINE_DIR=encpipeline_${USER} config

install: scripts config

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
            perl -c $$F && cp -upv $$F ${SCRIPTS}; \
	done

config: force
	mkdir -p ${PIPELINE_PATH}/${PIPELINE_DIR}/config
	cp -up config/{fields,cv}.ra ${PIPELINE_PATH}/${PIPELINE_DIR}/config

test: force
	cd test && ${MAKE}

force:

