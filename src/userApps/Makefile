#  Makefile for userApps project
#  performs the operations of fetching selected directories from
#  the kent source tree using 'git' and then building the utilities
#  in the kent source tree.  All build results will be kept locally
#  in this directory.

DESTDIR = ${CURDIR}
BINDIR = /bin
MACHTYPE = local
CGI_BIN = ${DESTDIR}/cgi-bin
DOCUMENTROOT = ${DESTDIR}/htdocs
SCRIPTS = ${DESTDIR}/scripts
SAMTABIXDIR = ${DESTDIR}/samtabix
USE_SAMTABIX = 1
USE_SSL = 1
NOSQLTEST = 1

envVars = DESTDIR=${DESTDIR} BINDIR=${BINDIR} MACHTYPE=${MACHTYPE} \
	CGI_BIN=${CGI_BIN} DOCUMENTROOT=${DOCUMENTROOT} SCRIPTS=${SCRIPTS} \
	SAMTABIXDIR=${SAMTABIXDIR} USE_SAMTABIX=1 USE_SSL=1 NOSQLTEST=1

all:  utils

utils: libs
	cd kent/src && ${envVars} ${MAKE} userApps
	cd kent/src/parasol && ${envVars} ${MAKE}
	./kent/src/utils/userApps/mkREADME.sh ${DESTDIR}/${BINDIR} ${CURDIR}/kentUtils.Documentation.txt

libs: part5Src.zip
	cd samtabix && ${envVars} ${MAKE}
	cd kent/src && ${envVars} ${MAKE} libs

clean:
	cd kent/src && ${envVars} ${MAKE} -i -k clean
	rm -f part1Src.zip part2Src.zip part3Src.zip part4Src.zip part5Src.zip
	rm -f userApps.zip

part5Src.zip: fetchSource

fetchSource:
	./fetchKentSource.sh
	@echo -n "size of fetched kent source tree: "
	@du -hs kent
	@echo -n "number of files in kent source tree: "
	@find ./kent -type f | wc -l

update: clean userApps
	${MAKE} utils

userApps:
	git archive --format=zip -9 \
	    --remote=git://genome-source.cse.ucsc.edu/kent.git \
	    HEAD src/userApps > userApps.zip
	unzip -q -o -d . -j userApps.zip 
