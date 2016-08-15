kentSrc = ../../..
# Build all directories in utils

include ../../../inc/common.mk

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
	    rm -f ${SCRIPTS}/$${F}; \
	    echo cp -p $$F ${SCRIPTS}; \
	    cp -p $$F ${SCRIPTS}; \
	 done

alpha: scripts

SCRIPT_FILES = \
	err.rb \
	verbose.rb \
	ra.rb 

clean::
	@echo nothing to clean in src/hg/utils/ruby/
