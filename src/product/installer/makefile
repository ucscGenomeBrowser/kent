kentSrc = ../..
include ../../inc/common.mk

GIT = git
GITUP = ${GIT} pull

user:
	${MAKE} doInstall destDir=${kentSrc}/hg/htdocs/goldenPath/help
	@echo ""
	@echo "Don't forget to cd to kent/src/hg/htdocs, commit goldenPath/help/gbic.html"
	@echo "and run your makes there!"
	@echo ""

doInstall:
	pandoc -t html5 -o ${destDir}/gbic.html README.md --template staticPage.html -t ../mirrorDocs/staticPage.lua
