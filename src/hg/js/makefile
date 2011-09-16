include ../../inc/common.mk

# recursive target to do install to DEST, with extra excludes EXCL.

JQUERY_FILES=jquery-1.2.6.pack.js jquery.js jquery.tablednd.js \
    jquery.imgareaselect.js jquery-ui.js jquery.contextmenu.js \
    ui.dropdownchecklist.js \
    jquery.cookie.js

JS_FILES=ajax.js autocomplete.js hgGateway.js hgHubConnect.js hgTracks.js \
    hui.js utils.js ddcl.js \
    encodeProject.js encodeDataSummary.js encodeDataMatrix.js encodeChipMatrix.js \
    ${JQUERY_FILES}

STATIC_FILES=staticDoc.js jquery.js

ifdef LOWELAB
    JS_FILES += lowetooltip.js loweautocomplete.js
endif

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/js/${USER}; \
		cp ${STATIC_FILES} ${DOCUMENTROOT}-${USER}/js; \
	else \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}/js/${USER}; \
	fi

alpha:
	${GITUP}
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

beta:
	${GITUP} 
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

clean:

compile:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

doInstall:
	@mkdir -p ${DEST}/
	../utils/cpAndLinkToResourceFiles.pl -exclude='makefile' -versionFile=../inc/versionInfo.h -destDir=${DEST} ${JS_FILES}
