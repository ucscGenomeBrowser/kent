kentSrc = ../../..
include ../../../inc/common.mk

# recursive target to do install to DEST, with extra excludes EXCL.

CSS_FILES=gbStatic.css
STYLE_DIR=staticStyle

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/${STYLE_DIR}; \
	fi

alpha:
	${GITUP}
	${MAKE} doInstall DEST=${DOCUMENTROOT}/${STYLE_DIR}

beta:
	${GITUP} 
	${MAKE} doInstall DEST=${DOCUMENTROOT}-beta/${STYLE_DIR}

clean:

compile:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/${STYLE_DIR}

doInstall:
	@mkdir -p ${DEST}/
	cp ${CSS_FILES} ${DEST}
