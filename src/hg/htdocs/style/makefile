include ../../../inc/common.mk

HTDOCS=/usr/local/apache/htdocs
HTDOCS_USER=${HTDOCS}-${USER}

# recursive target to do install to DEST, with extra excludes EXCL.

CSS_FILES=HGStyle.css autocomplete.css blueStyle.css floretStyle.css jquery-ui.css jquery.contextmenu.css ui.dropdownchecklist.css 
JS_FILES=ajax.js autocomplete.js hgGateway.js hgTracks.js hui.js utils.js ${JQUERY_FILES}

user:
	${MAKE} doInstall DEST=${HTDOCS_USER}/style

alpha:
	${GITUP}
	${MAKE} doInstall DEST=${HTDOCS}/style

beta:
	${GITUP} 
	${MAKE} doInstall DEST=${HTDOCS}/style

clean:

compile:

install:
	${MAKE} doInstall DEST=${HTDOCS}/style

doInstall:
	@mkdir -p ${DEST}/
	./cpAndLinkToResourceFiles.pl -exclude='makefile' -destDir=${DEST} ${CSS_FILES}
