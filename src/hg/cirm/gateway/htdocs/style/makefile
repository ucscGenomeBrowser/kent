kentSrc = ../../../../..
include ../../../../../inc/common.mk


CSS_FILES=cirmStyle.css

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/style; \
	fi

alpha:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style

clean:

compile:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style

doInstall:
	@mkdir -p ${DEST}/
	## recursive target to do install to DEST, with extra excludes EXCL.
	#../../utils/cpAndLinkToResourceFiles.pl ${extra} -destDir=${DEST} -versionFile=../../inc/versionInfo.h ${CSS_FILES}
	umask 002; rsync --update --times --recursive --omit-dir-times --relative --cvs-exclude --exclude='makefile' $(CSS_FILES) ${DEST}/
