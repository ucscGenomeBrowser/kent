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
	${CVSUP} -d -P cancerGenomics index.html
	${MAKE} doInstallCancerPublic DEST=${HTDOCS}

alphaCancerSU2C:
	${CVSUP} -d -P cancerGenomics index.html
	${MAKE} doInstallCancerSU2C DEST=${HTDOCS}

alphaCancerTCGA:
	${CVSUP} -d -P cancerGenomics index.html
	${MAKE} doInstallCancerTCGA DEST=${HTDOCS}

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

doInstallCancerSU2C:
	rsync -r --relative --exclude='makefile' ${EXCL} `cvsFiles` ${DEST}

	mv ${DEST}/index.html ${DEST}/index.html.org
	ln -sf ${DEST}/cancerGenomics/index.cancerGenomics.su2c.html ${DEST}/index.html
	mv index.html index.html.org

doInstallCancerTCGA:
	rsync -r --relative --exclude='makefile' ${EXCL} `cvsFiles` ${DEST}

	mv ${DEST}/index.html ${DEST}/index.html.org
	ln -sf ${DEST}/cancerGenomics/index.cancerGenomics.tcga.html ${DEST}/index.html
	mv index.html index.html.org
