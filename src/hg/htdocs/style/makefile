include ../../../inc/common.mk

# recursive target to do install to DEST, with extra excludes EXCL.

CSS_FILES=HGStyle.css autocomplete.css blueStyle.css floretStyle.css \
    hgHubConnect.css jquery-ui.css jquery.contextmenu.css \
    jquery.ui.autocomplete.css ui.dropdownchecklist.css encodeProject.css

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/style; \
	fi

alpha:
	${GITUP}
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style

beta:
	${GITUP} 
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style

clean:

compile:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style

doInstall:
	@mkdir -p ${DEST}/
	../../utils/cpAndLinkToResourceFiles.pl -destDir=${DEST} -versionFile=../../inc/versionInfo.h ${CSS_FILES}
