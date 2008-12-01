HTDOCS=/usr/local/apache/htdocs
HTDOCS_USER=${HTDOCS}-${USER}
CVSUP = cvs -q update 

user:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${HTDOCS_USER}

alpha:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${HTDOCS}

alphaCancerPublic:
	${CVSUP} -d -P
	${MAKE} doInstallCancerPublic DEST=${HTDOCS}

beta:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${HTDOCS} EXCL='--exclude=/eng'


# recursive target to do install to DEST, with extra excludes EXCL
# this queries CVS to find out the list of files to install, so
# avoid the usual crud that collects in directories.
doInstall:
	rsync -r --relative --exclude='makefile' ${EXCL} `cvsFiles` ${DEST}

#	ln -sf ENCODE ${DEST}/encode

doInstallCancerPublic:
	rsync -r --relative --exclude='makefile' ${EXCL} `cvsFiles` ${DEST}

	mv ${DEST}/index.html ${DEST}/index.html.org
	ln -sf ${DEST}/cancerGenomics/index.cancerGenomics.html ${DEST}/index.html
	mv index.html index.html.org
	
