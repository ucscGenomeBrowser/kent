kentSrc = ../../../../..
include ../../../../../inc/common.mk

CPFLAGS = -p ${CPREMDESTOPT}

# copies files to hgwdev or sandbox

TDOC_DIR=goldenPath/help/trackDb/

# when hub spec is versioned, increment version (e.g. change v0 to v1 (or v0a)), 
#   checkin new file (e.g. trackDbHub.v1.html), and add to file list

CURRENT_HUB_SPEC=v2

HUB_SPEC_FILES= trackDbHub.v0.html \
		trackDbHub.v1.html \
		trackDbHub.v2.html \
		trackDbHub.v3.html

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
	cp ${CPFLAGS} trackDbLibrary.shtml ${DEST}
	cp ${CPFLAGS} trackDbDoc.js ${DEST}
	cp ${CPFLAGS} trackDbDoc.css ${DEST}
	cp ${CPFLAGS} trackDbDoc.html ${DEST}
	cp ${CPFLAGS} trackDbHub.html ${DEST}
	cp ${CPFLAGS} changes.html ${DEST}
	cp ${CPFLAGS} ${HUB_SPEC_FILES} ${DEST}
