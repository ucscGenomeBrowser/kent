kentSrc = ..
include ../src/inc/common.mk

GIT = git
GITUP = ${GIT} pull

user:
	${MAKE} doInstall destDir=${DOCUMENTROOT}-${USER}

doInstall:
	mkdir -p ${destDir}/docs/
	pandoc --ascii -f markdown-smart -t html5 -o ${destDir}/docs/index.html index.md --template staticPage.html -t staticPage.lua
	chmod a+x ${destDir}/docs/index.html

testPandoc:
	@printf "Test the following path and commands look ok\n"
	@printf "pandoc path: %s\n" "`which pandoc`"
	@printf "pandoc --ascii -f markdown-smart -t html5 index.md --template staticPage.html -t staticPage.lua\n" "${DOCUMENTROOT}/"
