# Build encode loader

include ../../../inc/common.mk

SCRIPT_FILES = \
	encodeMkGeoPkg \
	encodeAddGeoAccessions

all:
	@${MAKE} --no-print-directory SCRIPTS=${HOME}/bin/scripts scripts

alpha: scripts

install: all

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
            cp -upv $$F ${SCRIPTS}; \
	done

clean::
	echo nothing to clean in hg/encode/encodeMkGeoPkg/
