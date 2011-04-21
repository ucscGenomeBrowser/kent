include ../../inc/common.mk

# NOTES:
#  - much of this is current repeats in the hgdownload htdocs makefile
#  - all symlinks are built by the doSymLink target


GIT = git
GITUP = ${GIT} pull
FIND=gitFiles

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall destDir=${DOCUMENTROOT}-${USER} extra=doSymLink; \
	fi

alpha:
	${MAKE} doInstall destDir=${DOCUMENTROOT} extra=doSymLink

beta:
	${GITUP}
	${MAKE} doInstall destDir=${DOCUMENTROOT} extra=doSymLink

alphaCancerPublic:
	${MAKE} doInstallCancerPublic destDir=${DOCUMENTROOT} extra=doSymLink

alphaCancerSU2C:
	${MAKE} doInstallCancerSU2C destDir=${DOCUMENTROOT} extra=doSymLink

alphaCancerTCGA:
	${MAKE} doInstallCancerTCGA destDir=${DOCUMENTROOT} extra=doSymLink

# Recursive target to do install to destDir, with extra excludes excl and
# specific includes in incl.  The variable extra are extra targets to depend
# on.  This queries git to find out the list of files to install, so avoid the
# usual crud that collects in directories.
doInstall: doRsync ${extra}

doInstallCancerPublic: doRsync ${extra}
	mv ${destDir}/index.html ${destDir}/index.html.org
	ln -sf ${destDir}/cancerGenomics/index.cancerGenomics.html ${destDir}/index.html
	mv index.html index.html.org

doInstallCancerSU2C: doRsync ${extra}
	mv ${destDir}/index.html ${destDir}/index.html.org
	ln -sf ${destDir}/cancerGenomics/index.cancerGenomics.su2c.html ${destDir}/index.html
	mv index.html index.html.org

doInstallCancerTCGA: doRsync ${extra}
	mv ${destDir}/index.html ${destDir}/index.html.org
	ln -sf ${destDir}/cancerGenomics/index.cancerGenomics.tcga.html ${destDir}/index.html
	mv index.html index.html.org

# Run rsync.  Care is taking to preserve times and mode on files.  rsync is
# smart about recreating file under the current users so that times and
# permissions maybe modified.  However, problems happen when a directory
# is not owned by the current users.  These options attemp to work around it.
# use care in changing
rsyncOpts = --times --recursive --omit-dir-times --relative --cvs-exclude --exclude='makefile' --exclude='cpAndLinkToResourceFiles.pl' --exclude='style/*'
doRsync:
	umask 002; rsync ${rsyncOpts} ${excl} ${incl} `$(FIND)` ${destDir}

# build symbolic links
doSymLink: doRsync
	ln -sfn ../trash ${destDir}/trash
	ln -sfn goldenPath ${destDir}/goldenpath
	ln -sfn /gbdb/visiGene ${destDir}/visiGene
	ln -sfn ENCODE ${destDir}/encode
	ln -sfn Branchiostoma_lanceolatum.jpg ${destDir}/images/Branchiostoma_floridae.jpg
	ln -sfn Rhesus_macaque.jpg ${destDir}/images/Macaca_mulatta.jpg
	mkdir -p ${destDir}/RNA-img
	ln -sfn /gbdb/hg18/RNA-img ${destDir}/RNA-img/hg18
	ln -sfn /gbdb/hg19/RNA-img ${destDir}/RNA-img/hg19
	ln -sfn /gbdb/mm9/RNA-img  ${destDir}/RNA-img/mm9
	ln -sfn /gbdb/rn4/RNA-img  ${destDir}/RNA-img/rn4

# Other targets that may be invoked by ../makefile:
compile:

install: alpha

clean:
