kentSrc = ../../../..
include ../../../../inc/common.mk

#Get list of files to copy over. Will only copy over files which are being tracked by git.
FIND=gitFiles

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}; \
	fi

alpha:
	${MAKE} doInstall DEST=${DOCUMENTROOT}

clean:

compile:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}

doInstall:
	@mkdir -p ${DEST}/
	umask 002; rsync --times --recursive --omit-dir-times --relative --cvs-exclude --exclude='makefile' --exclude='style/*' `$(FIND)` ${DEST}/


