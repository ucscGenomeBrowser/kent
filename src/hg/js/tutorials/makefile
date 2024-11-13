kentSrc = ../../..
include ../../../inc/common.mk
include ../install.mk

SYMLINKED_FILES=tutorial.js clinicalTutorial.js hgTracksPopup.js

doInstall:
	@mkdir -p ${DEST}/
	../../utils/cpAndLinkToResourceFiles.pl ${extra} -exclude='makefile' \
			-versionFile=../../inc/versionInfo.h -destDir=${DEST} ${SYMLINKED_FILES}
# Left this here in case we have static tutorial files in the future
#	for file in ${STATIC_FILES}; do \
#	    rsync -a $$file ${DEST}/; \
#	done
