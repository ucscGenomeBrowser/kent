# Build encode validator

include ../../../inc/common.mk

install: scripts config

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
	    rm -f ${SCRIPTS}/$${F}; \
	    echo cp -p $$F ${SCRIPTS}; \
	    cp -p $$F ${SCRIPTS}; \
	done

PIPELINE_DIR = /cluster/data/encode/pipeline/encpipeline_kate
#PIPELINE_DIR = /cluster/data/encode/pipeline/encpipeline_dev

config: force
	cp -rp config ${PIPELINE_DIR}

alpha: scripts

SCRIPT_FILES = \
	doEncodeValidate.pl

test: force
	cd test && ${MAKE}

force:

