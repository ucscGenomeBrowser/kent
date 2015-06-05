include ../../../../../inc/common.mk

# copies files to hgwdev or sandbox

TDOC_DIR=goldenPath/help/trackDb/

# when hub spec is versioned, increment version (e.g. change v0 to v1 (or v0a)), 
#   checkin new file (e.g. trackDbHub.v1.html), and add to file list

CURRENT_HUB_SPEC=v0

HUB_SPEC_FILES= trackDbHub.v0.html

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
	echo ${CURRENT_HUB_SPEC}
	cp trackDbLibrary.shtml ${DEST}
	cp trackDbDoc.js ${DEST}
	cp trackDbDoc.css ${DEST}
	cp trackDbDoc.html ${DEST}
	rm -f ${DEST}/trackDbHub.html
	cp ${HUB_SPEC_FILES} ${DEST}
	ln -s trackDbHub.${CURRENT_HUB_SPEC}.html ${DEST}/trackDbHub.html
