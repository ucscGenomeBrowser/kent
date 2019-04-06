kentSrc = ../../../..
include ../../../../inc/common.mk

#Get list of files to copy over. 

# STATIC files must pass jshint
STATIC_FILES=cirmStuff.js

user:
	@if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/js; \
	else \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}/js/${USER}; \
	fi

alpha:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js


clean:

compile:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

doInstall:
	@mkdir -p ${DEST}/
	for file in ${STATIC_FILES}; do \
	    rsync -tOW $$file ${DEST}/; \
	done

