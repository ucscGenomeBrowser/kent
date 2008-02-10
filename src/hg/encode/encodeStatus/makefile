# Build encode validator

include ../../../inc/common.mk

SCRIPT_FILES = \
	encodeStatus.csh

PIPELINE_DIR=encpipeline_beta

all:  
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts scripts

alpha: scripts config

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
            cp -upv $$F ${SCRIPTS}; \
	done

config: force
	mkdir -p ${PIPELINE_PATH}/${PIPELINE_DIR}/config
	cp -up config/{fields,cv}.ra ${PIPELINE_PATH}/${PIPELINE_DIR}/config

test: force
	cd test && ${MAKE}

force:

