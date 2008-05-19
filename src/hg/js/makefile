include ../../inc/common.mk

CVSUP = cvs -q update 

# recursive target to do install to DEST, with extra excludes EXCL
# this queries CVS to find out the list of files to install, so
# avoid the usual crud that collects in directories.

user:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js/${USER}

alpha:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

beta:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

clean:

compile:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

doInstall:
	@mkdir -p ${DEST}/
	rsync -t -r --relative --exclude='makefile' ${EXCL} `cvsFiles` ${DEST}/
