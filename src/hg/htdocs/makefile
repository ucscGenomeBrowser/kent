
HTDOCS=/usr/local/apache/htdocs
HTDOCS_USER=${HTDOCS}-${USER}

all:
	${MAKE} doInstall DEST=${HTDOCS_USER}

alpha:
	${MAKE} doInstall DEST=${HTDOCS}

beta:
	${MAKE} doInstall DEST=${HTDOCS} EXCL='--exclude=/eng'


# recursive target to do install to DEST, with extra excludes EXCL
# this queries CVS to find out the list of files to install, so
# avoid the usual crud that collects in directories.
doInstall:
	rsync -t -r --relative --exclude='makefile' ${EXCL} `cvsFiles` ${DEST}

#	ln -sf ENCODE ${DEST}/encode
