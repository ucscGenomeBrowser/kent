# Build encode validator

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

SCRIPT_FILES = \
	doEncodeValidate.pl


