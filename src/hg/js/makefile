include ../../inc/common.mk

CVSUP = cvs -q update 

# recursive target to do install to DEST, with extra excludes EXCL
# this queries CVS to find out the list of files to install, so
# avoid the usual crud that collects in directories.

user:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${CGI_BIN}-${USER}/js

alpha:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${CGI_BIN}/js

beta:
	${CVSUP} -d -P
	${MAKE} doInstall DEST=${CGI_BIN}/js

clean:
	

compile:
	

install:
	

doInstall:
	rsync -t -r --relative --exclude='makefile' ${EXCL} `cvsFiles` ${DEST}/
