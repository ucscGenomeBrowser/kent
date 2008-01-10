# Build encode loader

include ../../../inc/common.mk

install: scripts

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
	    rm -f ${SCRIPTS}/$${F}; \
	    echo cp -p $$F ${SCRIPTS}; \
	    cp -p $$F ${SCRIPTS}; \
	done

alpha: scripts

test:
	doEncodeLoad.rb load /cluster/store3/encpipeline_dev/15/43


SCRIPT_FILES = \
	doEncodeLoad.rb


