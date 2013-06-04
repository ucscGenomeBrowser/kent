#  Makefile for userApps project
#  performs the operations of fetching selected directories from
#  the kent source tree using 'git' and then building the utilities
#  in the kent source tree.  All build results will be kept locally
#  in this directory.

export DESTDIR = ${CURDIR}
export BINDIR = /bin
export MACHTYPE = local
export CGI_BIN = ${DESTDIR}/cgi-bin
export DOCUMENTROOT = ${DESTDIR}/htdocs
export SCRIPTS = ${DESTDIR}/scripts
export SAMTABIXDIR = ${DESTDIR}/samtabix
export USE_SAMTABIX = 1
export USE_SSL = 1
export NOSQLTEST = 1

all:  utils

utils: libs
	cd kent/src && ${MAKE} userApps
	cd kent/src/parasol && ${MAKE}
	./kent/src/utils/userApps/mkREADME.sh ${DESTDIR}/${BINDIR} ${CURDIR}/kentUtils.Documentation.txt

libs:
	cd samtabix && ${MAKE}
	cd kent/src && ${MAKE} libs

clean:
	cd kent/src && ${MAKE} -i -k clean
	rm -f part1Src.zip part2Src.zip part3Src.zip part4Src.zip part5Src.zip
	rm -f userApps.zip

fetchSource:
	./fetchKentSource.sh
	@echo -n "size of fetched kent source tree: "
	@du -hs kent
	@echo -n "number of files in kent source tree: "
	@find ./kent -type f | wc -l
	@${MAKE} installEnvironment

# this installEnvironment will add all the shell environment variables
# to the kent/src/inc/localEnvironment.mk file which is included
# from the kent/src/inc/userApps.mk to allow any 'make' to function
# properly when inside this extracted source tree.

installEnvironment:
	@echo -e "export DESTDIR = ${DESTDIR}\n\
export BINDIR = ${BINDIR}\n\
export MACHTYPE = ${MACHTYPE}\n\
export CGI_BIN = ${CGI_BIN}\n\
export DOCUMENTROOT = ${DOCUMENTROOT}\n\
export SCRIPTS = ${SCRIPTS}\n\
export SAMTABIXDIR = ${SAMTABIXDIR}\n\
export USE_SAMTABIX = 1\n\
export USE_SSL = 1\n\
export NOSQLTEST = 1" > kent/src/inc/localEnvironment.mk

update: clean userApps
	${MAKE} fetchSource
	${MAKE} utils

userApps:
	git archive --format=zip -9 \
	    --remote=git://genome-source.cse.ucsc.edu/kent.git \
	    HEAD src/userApps > userApps.zip
	unzip -q -o -d . -j userApps.zip 
