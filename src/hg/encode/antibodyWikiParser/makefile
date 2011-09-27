# Build encode validator

include ../../../inc/common.mk

SCRIPT_FILES = \
	antibodyWikiParser.py

all:	wranglerTools
	

wranglerTools:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
            cp -upv $$F ${SCRIPTS}; \
	done

clean::
	echo nothing to clean in hg/encode/antibodyWikiParser/
