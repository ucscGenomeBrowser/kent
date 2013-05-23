#  Makefile for userApps project
#  performs the operations of fetching selected directories from
#  the kent source tree using 'git' and then building the utilities
#  in the kent source tree.

samtabixDir = ${CURDIR}/samtabix
DESTDIR = ${CURDIR}
BINDIR = /bin
extraCFlags =
mysqlInc =
mysqlLibs =
pngLib =

fetchSource:
	./fetchKentSource.sh
	@echo -n "size of fetched kent source tree: "
	@du -hs kent
	@echo -n "number of files in kent source tree: "
	@find ./kent -type f | wc -l
