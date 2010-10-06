include ../../../inc/common.mk

# recursive target to do install to DEST, with extra excludes EXCL.

CSS_FILES=HGStyle.css autocomplete.css blueStyle.css floretStyle.css jquery-ui.css jquery.contextmenu.css ui.dropdownchecklist.css ui.tabs.css
JS_FILES=ajax.js autocomplete.js hgGateway.js hgTracks.js hui.js utils.js ${JQUERY_FILES}

user:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style/${USER}

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
	./cpAndLinkToResourceFiles.pl -exclude='makefile' -destDir=${DEST} ${CSS_FILES}
