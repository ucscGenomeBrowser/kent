include ../../../../../inc/common.mk

# copies files to hgwdev or sandbox

TDOC_DIR=goldenPath/help/trackDb/

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/${TDOC_DIR}; \
	else \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}/${TDOC_DIR}; \
	fi

alpha:
	${GITUP}
	${MAKE} doInstall DEST=${DOCUMENTROOT}/${TDOC_DIR}

clean:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/${TDOC_DIR}

doInstall:
	cp trackDbLibrary.shtml ${DEST}
	cp trackDbDoc.js ${DEST}
	cp trackDbDoc.css ${DEST}
	cp *.html ${DEST}
