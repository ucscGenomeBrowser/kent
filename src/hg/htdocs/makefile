
HTDOCS=/usr/local/apache/htdocs
HTDOCS_USER=${HTDOCS}-${USER}

all:
	${MAKE} doInstall DEST=${HTDOCS_USER}

alpha:
	${MAKE} doInstall DEST=${HTDOCS}

beta:
	${MAKE} doInstall DEST=${HTDOCS} EXCL='--exclude=/eng'


# recursive target to do install to DEST, with extra excludes EXCL
doInstall:
	rsync -ra --cvs-exclude ${EXCL} . ${DEST}
