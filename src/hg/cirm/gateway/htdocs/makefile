kentSrc = ../../..
include ../../../../inc/common.mk

#Get list of files to copy over. Will only copy over files which have been committed.
FIND=gitFiles
HOSTNAME:=$(shell hostname)

# Set proper htdocs directory based on what machine we're using
ifeq (${HOSTNAME}, hgwdev)
  HTDOCSDIR=${DOCUMENTROOT}-cirm
endif
ifeq (${HOSTNAME}, cirm)
  HTDOCSDIR=${DOCUMENTROOT}
endif
ifeq (${HOSTNAME}, cirm-01)
  HTDOCSDIR=${DOCUMENTROOT}
endif

docs:
	umask 002; rsync --times --recursive --omit-dir-times --relative --cvs-exclude --exclude='makefile' --exclude='style/*' `$(FIND)` ${HTDOCSDIR}

