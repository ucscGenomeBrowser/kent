kentSrc = ../..
include ../../inc/common.mk

GIT = git
GITUP = ${GIT} pull

user:
	${MAKE} doInstall destDir=${kentSrc}/hg/htdocs
	@echo ""
	@echo "Don't forget to cd to kent/src/hg/htdocs, commit goldenPath/help/mirror.html"
	@echo "and goldenPath/help/mirrorManual.html, and run your makes from there!"
	@echo ""

doInstall:
	pandoc -t html5 -o ${destDir}/goldenPath/help/mirror.html ../README.txt --template staticPage.html -t staticPage.lua
	pandoc -t html5 -o ${destDir}/goldenPath/help/mirrorManual.html ../mirrorManual.txt --template staticPage.html -t staticPage.lua
