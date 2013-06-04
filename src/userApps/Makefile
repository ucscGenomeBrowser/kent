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

update: clean userApps
	${MAKE} fetchSource
	${MAKE} utils

userApps:
	git archive --format=zip -9 \
	    --remote=git://genome-source.cse.ucsc.edu/kent.git \
	    HEAD src/userApps > userApps.zip
	unzip -q -o -d . -j userApps.zip 
