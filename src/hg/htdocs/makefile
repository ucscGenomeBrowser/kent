HTDOCS=/usr/local/apache/htdocs
HTDOCS_USER=${HTDOCS}-${USER}
HTDOCS_GENECATS=/usr/local/apache/htdocs-genecats

CVS = cvs
CVSUP = ${CVS} -q update -d -P

# NOTES:
#  - all symlinks are built in the doInstall target

user:
	${CVSUP}
	${MAKE} doInstall DEST=${HTDOCS_USER} EXTRA=doSymLink

alpha:
	${CVSUP}
	${MAKE} doInstall DEST=${HTDOCS} EXTRA=doSymLink

beta:
	${CVSUP}
	${MAKE} doInstall DEST=${HTDOCS} EXTRA=doSymLink

alphaCancerPublic:
	${CVSUP} cancerGenomics index.html
	${MAKE} doInstallCancerPublic DEST=${HTDOCS} EXTRA=doSymLink

alphaCancerSU2C:
	${CVSUP} cancerGenomics index.html
	${MAKE} doInstallCancerSU2C DEST=${HTDOCS} EXTRA=doSymLink

alphaCancerTCGA:
	${CVSUP} cancerGenomics index.html
	${MAKE} doInstallCancerTCGA DEST=${HTDOCS} EXTRA=doSymLink

# Recursive target to do install to DEST, with extra excludes EXCL and
# specific includes in INCL.  The variable EXTRA are extra targets to depend
# on.  This queries CVS to find out the list of files to install, so avoid the
# usual crud that collects in directories.
doInstall: doRsync ${EXTRA}

doInstallCancerPublic: doRsync ${EXTRA}
	mv ${DEST}/index.html ${DEST}/index.html.org
	ln -sf ${DEST}/cancerGenomics/index.cancerGenomics.html ${DEST}/index.html
	mv index.html index.html.org

doInstallCancerSU2C: doRsync ${EXTRA}
	mv ${DEST}/index.html ${DEST}/index.html.org
	ln -sf ${DEST}/cancerGenomics/index.cancerGenomics.su2c.html ${DEST}/index.html
	mv index.html index.html.org

doInstallCancerTCGA: doRsync ${EXTRA}
	mv ${DEST}/index.html ${DEST}/index.html.org
	ln -sf ${DEST}/cancerGenomics/index.cancerGenomics.tcga.html ${DEST}/index.html
	mv index.html index.html.org

# Run rsync.  Care is taking to preserve times and mode on files.  rsync is
# smart about recreating file under the current users so that times and
# permissions maybe modified.  However, problems happen when a directory
# is not owned by the current users.  These options attemp to work around it.
# use care in changing
doRsync:
	umask 002; rsync --times --recursive --executability --omit-dir-times --relative --cvs-exclude --exclude='makefile' ${EXCL} ${INCL} `cvsFiles` ${DEST}

# build symbolic links
doSymLink: doRsync
	ln -sfn ../trash ${DEST}/trash
	ln -sfn goldenPath ${DEST}/goldenpath
	ln -sfn /gbdb/visiGene ${DEST}/visiGene
	ln -sfn ENCODE ${DEST}/encode
	ln -sfn Branchiostoma_lanceolatum.jpg ${DEST}/images/Branchiostoma_floridae.jpg
	mkdir -p ${DEST}/RNA-img
	ln -sfn /gbdb/hg18/RNA-img ${DEST}/RNA-img/hg18
	ln -sfn /gbdb/hg19/RNA-img ${DEST}/RNA-img/hg19
	ln -sfn /gbdb/mm9/RNA-img  ${DEST}/RNA-img/mm9
	ln -sfn /gbdb/rn4/RNA-img  ${DEST}/RNA-img/rn4
