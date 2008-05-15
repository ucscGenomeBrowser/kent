
HTDOCS=/usr/local/apache/htdocs
HTDOCS_USER=${HTDOCS}-${USER}
CVSUP = cvs -q update 

user:
	@echo "only 'make alpha' is allowed here"
	@exit 1

alpha:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${HTDOCS}

beta:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${HTDOCS} EXCL='--exclude=/eng'


# recursive target to do install to DEST, with extra excludes EXCL
# this queries CVS to find out the list of files to install, so
# avoid the usual crud that collects in directories.
doInstall:
	rsync -t -r --relative --exclude='makefile' ${EXCL} `cvsFiles` ${DEST}

#	ln -sf ENCODE ${DEST}/encode
