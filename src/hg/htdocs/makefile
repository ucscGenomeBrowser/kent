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
doInstall: doRsync
#	ln -sf ENCODE ${DEST}/encode

doInstallCancerPublic: doRsync
	mv ${DEST}/index.html ${DEST}/index.html.org
	ln -sf ${DEST}/cancerGenomics/index.cancerGenomics.html ${DEST}/index.html
	mv index.html index.html.org

doInstallCancerSU2C: doRsync
	mv ${DEST}/index.html ${DEST}/index.html.org
	ln -sf ${DEST}/cancerGenomics/index.cancerGenomics.su2c.html ${DEST}/index.html
	mv index.html index.html.org

doInstallCancerTCGA: doRsync
	mv ${DEST}/index.html ${DEST}/index.html.org
	ln -sf ${DEST}/cancerGenomics/index.cancerGenomics.tcga.html ${DEST}/index.html
	mv index.html index.html.org

# Run rsync.  Care is taking to preserve times and mode on files.  rsync is
# smart about recreating file under the current users so that times and
# permissions maybe modified.  However, problems happen when a directory
# is not owned by the current users.  These options attemp to work around it.
# use create care in changing
doRsync:
	umask 002; rsync --times --recursive --executability --omit-dir-times --relative --exclude='makefile' ${EXCL} `cvsFiles` ${DEST}
