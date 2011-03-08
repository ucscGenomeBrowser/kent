include ../../inc/common.mk

# recursive target to do install to DEST, with extra excludes EXCL.

JQUERY_FILES=jquery-1.2.6.pack.js jquery.js jquery.tablednd.js jquery.imgareaselect.js jquery-ui.js jquery.contextmenu.js jquery.autocomplete.js ui.core.js ui.dropdownchecklist.js
JS_FILES=ajax.js autocomplete.js hgGateway.js hgTracks.js hui.js utils.js ${JQUERY_FILES}
ifdef LOWELAB
    JS_FILES += lowetooltip.js loweautocomplete.js
endif

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/js/${USER}; \
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
	./cpJsFiles.pl -exclude='makefile' -destDir=${DEST} ${JS_FILES}
