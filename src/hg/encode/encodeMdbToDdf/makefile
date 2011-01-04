# Build encode validator

include ../../../inc/common.mk

SCRIPT_FILES = \
	encodeMdbToDdf

install: 
	@${MAKE} --no-print-directory SCRIPTS=${SCRIPTS} scripts

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
            perl -c $$F && cp -upv $$F ${SCRIPTS}; \
	done

clean::
	echo nothing to clean in hg/encode/encodeMdbToDdf/
