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
	@echo "Or run this:"
	@echo "git add ../mirrorManual.txt ../../hg/htdocs/mirror.html ../../hg/htdocs/goldenPath/help/mirrorManual.html"
	@echo "git commit"


doInstall:
	pandoc -t html5 -o ${destDir}/goldenPath/help/mirror.html ../README.txt --template staticPage.html -t staticPage.lua
	pandoc -t html5 -o ${destDir}/goldenPath/help/mirrorManual.html ../mirrorManual.txt --template staticPage.html -t staticPage.lua

testPandoc:
	@printf "Test the following path and commands look ok\n"
	@printf "pandoc path: %s\n" "`which pandoc`"
	@printf "pandoc -t html5 ../README.txt --template staticPage.html -t staticPage.lua\n" "${kentSrc}/hg/htdocs/"
	@printf "pandoc -t html5 ../mirrorManual.txt --template staticPage.html -t staticPage.lua\n" "${kentSrc}/hg/htdocs"
