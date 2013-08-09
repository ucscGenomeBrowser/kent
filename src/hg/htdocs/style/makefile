include ../../../inc/common.mk

# recursive target to do install to DEST, with extra excludes EXCL.

CSS_FILES=HGStyle.css autocomplete.css blueStyle.css encodeProject.css floretStyle.css \
    hgHubConnect.css jquery-ui.css jquery.contextmenu.css \
    jquery.ui.autocomplete.css theme-modern.css ui.dropdownchecklist.css nice_menu.css \
    encodeDataMatrix.css encodeDataSummary.css userAccounts.css

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/style; \
	fi

alpha:
	${GITUP}
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style

beta:
	${GITUP} 
	${MAKE} doInstall DEST=${DOCUMENTROOT}-beta/style extra=-forceVersionNumbers

clean:

compile:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style

doInstall:
	@mkdir -p ${DEST}/
	../../utils/cpAndLinkToResourceFiles.pl ${extra} -destDir=${DEST} -versionFile=../../inc/versionInfo.h ${CSS_FILES}
