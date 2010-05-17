HTDOCS=/usr/local/apache/htdocs
HTDOCS_USER=${HTDOCS}-${USER}
CVSUP = cvs -q update 

# NOTES:
#  - all symlinks are built in the doInstall target


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
	ln -sf ../trash ${DEST}/trash	
	ln -sf goldenPath ${DEST}/goldenpath
	ln -sf /gbdb/visiGene ${DEST}/visiGene
	ln -sf ENCODE ${DEST}/encode
	ln -sf Branchiostoma_lanceolatum.jpg ${DEST}/images/Branchiostoma_floridae.jpg
	mkdir -p ${DEST}/RNA-img
	ln -sf /gbdb/hg18/RNA-img ${DEST}/RNA-img/hg18
	ln -sf /gbdb/hg19/RNA-img ${DEST}/RNA-img/hg19
	ln -sf /gbdb/mm9/RNA-img  ${DEST}/RNA-img/mm9
	ln -sf /gbdb/rn4/RNA-img  ${DEST}/RNA-img/rn4

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
