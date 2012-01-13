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
	ln -sfn /gbdb/ailMel1/RNA-img  ${destDir}/RNA-img/ailMel1
	ln -sfn /gbdb/caePb1/RNA-img  ${destDir}/RNA-img/caePb1
	ln -sfn /gbdb/canFam2/RNA-img  ${destDir}/RNA-img/canFam2
	ln -sfn /gbdb/danRer5/RNA-img  ${destDir}/RNA-img/danRer5
	ln -sfn /gbdb/felCat3/RNA-img  ${destDir}/RNA-img/felCat3
	ln -sfn /gbdb/gorGor3/RNA-img  ${destDir}/RNA-img/gorGor3
	ln -sfn /gbdb/ornAna1/RNA-img  ${destDir}/RNA-img/ornAna1
	ln -sfn /gbdb/panTro3/RNA-img  ${destDir}/RNA-img/panTro3
	ln -sfn /gbdb/susScr2/RNA-img  ${destDir}/RNA-img/susScr2
	ln -sfn /gbdb/anoCar2/RNA-img  ${destDir}/RNA-img/anoCar2
	ln -sfn /gbdb/caePb2/RNA-img  ${destDir}/RNA-img/caePb2
	ln -sfn /gbdb/cavPor3/RNA-img  ${destDir}/RNA-img/cavPor3
	ln -sfn /gbdb/danRer6/RNA-img  ${destDir}/RNA-img/danRer6
	ln -sfn /gbdb/felCat4/RNA-img  ${destDir}/RNA-img/felCat4
	ln -sfn /gbdb/loxAfr3/RNA-img  ${destDir}/RNA-img/loxAfr3
	ln -sfn /gbdb/oryCun2/RNA-img  ${destDir}/RNA-img/oryCun2
	ln -sfn /gbdb/petMar1/RNA-img  ${destDir}/RNA-img/petMar1
	ln -sfn /gbdb/taeGut1/RNA-img  ${destDir}/RNA-img/taeGut1
	ln -sfn /gbdb/bosTau4/RNA-img  ${destDir}/RNA-img/bosTau4
	ln -sfn /gbdb/caeRem2/RNA-img  ${destDir}/RNA-img/caeRem2
	ln -sfn /gbdb/cb3/RNA-img  ${destDir}/RNA-img/cb3
	ln -sfn /gbdb/danRer7/RNA-img  ${destDir}/RNA-img/danRer7
	ln -sfn /gbdb/fr2/RNA-img  ${destDir}/RNA-img/fr2
	ln -sfn /gbdb/melGal1/RNA-img  ${destDir}/RNA-img/melGal1
	ln -sfn /gbdb/oryLat2/RNA-img  ${destDir}/RNA-img/oryLat2
	ln -sfn /gbdb/ponAbe2/RNA-img  ${destDir}/RNA-img/ponAbe2
	ln -sfn /gbdb/tetNig2/RNA-img  ${destDir}/RNA-img/tetNig2
	ln -sfn /gbdb/bosTau6/RNA-img  ${destDir}/RNA-img/bosTau6
	ln -sfn /gbdb/caeRem3/RNA-img  ${destDir}/RNA-img/caeRem3
	ln -sfn /gbdb/ce4/RNA-img  ${destDir}/RNA-img/ce4
	ln -sfn /gbdb/equCab1/RNA-img  ${destDir}/RNA-img/equCab1
	ln -sfn /gbdb/galGal3/RNA-img  ${destDir}/RNA-img/galGal3
	ln -sfn /gbdb/monDom5/RNA-img  ${destDir}/RNA-img/monDom5
	ln -sfn /gbdb/oviAri1/RNA-img  ${destDir}/RNA-img/oviAri1
	ln -sfn /gbdb/priPac1/RNA-img  ${destDir}/RNA-img/priPac1
	ln -sfn /gbdb/xenTro3/RNA-img  ${destDir}/RNA-img/xenTro3
	ln -sfn /gbdb/caeJap1/RNA-img  ${destDir}/RNA-img/caeJap1
	ln -sfn /gbdb/calJac3/RNA-img  ${destDir}/RNA-img/calJac3
	ln -sfn /gbdb/ce6/RNA-img  ${destDir}/RNA-img/ce6
	ln -sfn /gbdb/equCab2/RNA-img  ${destDir}/RNA-img/equCab2
	ln -sfn /gbdb/gasAcu1/RNA-img  ${destDir}/RNA-img/gasAcu1
	ln -sfn /gbdb/nomLeu1/RNA-img  ${destDir}/RNA-img/nomLeu1
	ln -sfn /gbdb/panTro2/RNA-img  ${destDir}/RNA-img/panTro2
	ln -sfn /gbdb/rheMac2/RNA-img  ${destDir}/RNA-img/rheMac2


# Other targets that may be invoked by ../makefile:
compile:

install: alpha

clean:
