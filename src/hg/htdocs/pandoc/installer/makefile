kentSrc = ../../../..
include ../../../../inc/common.mk

GIT = git
GITUP = ${GIT} pull

ifeq (${DOCUMENTROOT_USER},)
    DOCUMENTROOT_USER=${DOCUMENTROOT}-${USER}
endif

user:
	if test -d ${DOCUMENTROOT_USER}; then \
		${MAKE} doInstall destDir=${DOCUMENTROOT_USER} ; \
	fi

alpha:
	${MAKE} doInstall destDir=${DOCUMENTROOT} 

beta:
	${GITUP}
#	${MAKE} doInstall destDir=qateam@hgwbeta:${DOCUMENTROOT} 
	${MAKE} doInstall destDir=${DOCUMENTROOT}-beta
	rsync ${rsyncOpts} ${DOCUMENTROOT}-beta/goldenPath/help/gbic.html qateam@hgwbeta:${DOCUMENTROOT}/goldenPath/help/

doInstall:
	pandoc -t html5 -o ${destDir}/goldenPath/help/gbic.html README.md --template staticPage.html -t ../mirrorDocs/staticPage.lua 

# Run rsync.  Care is taking to preserve times and mode on files.  rsync is
# smart about recreating file under the current users so that times and
# permissions maybe modified.  However, problems happen when a directory
# is not owned by the current users.  These options attemp to work around it.
# use care in changing
rsyncOpts = --times --omit-dir-times --cvs-exclude --no-relative
