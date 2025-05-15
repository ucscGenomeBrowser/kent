kentSrc = ..
include ../src/inc/common.mk

user:
	${MAKE} doInstall destDir=${DOCUMENTROOT}-${USER}

alpha:
	${MAKE} doInstall destDir=${DOCUMENTROOT}

beta:
	${MAKE} doInstall destDir=${DOCUMENTROOT}-beta

DOCS = index gb101 gatewayTutorial page_template


doInstall: ${DOCS:%=%_install}

%_install: 
	${MKDIR} ${destDir}/docs/
	rm -f  ${destDir}/docs/$*.tmp.html
	pandoc --ascii -f markdown-smart -t html5 -o ${destDir}/docs/$*.tmp.html $*.md --template staticPage.html -t staticPage.lua --lua-filter GBpandocHTMLrules.lua
	mv -f  ${destDir}/docs/$*.tmp.html ${destDir}/docs/$*.html
	chmod a+x ${destDir}/docs/$*.html

testPandoc:
	@printf "Test the following path and commands look ok\n"
	@printf "pandoc path: %s\n" "`which pandoc`"
	@printf "pandoc --ascii -f markdown-smart -t html5 index.md --template staticPage.html -t staticPage.lua\n" "${DOCUMENTROOT}/"
