kentSrc = ../..
include ../../inc/common.mk

all:
	pandoc -t html5 -o $(DOCUMENTROOT)-$(USER)/goldenPath/help/gbic.html README.md --template staticPage.html -t ../mirrorDocs/staticPage.lua
	chmod a+x $(DOCUMENTROOT)-$(USER)/goldenPath/help/gbic.html
alpha:
	pandoc -t html5 -o $(DOCUMENTROOT)/goldenPath/help/gbic.html README.md --template staticPage.html -t ../mirrorDocs/staticPage.lua
	chmod a+x $(DOCUMENTROOT)/goldenPath/help/gbic.html
beta:
	pandoc -t html5 -o $(DOCUMENTROOT)-beta/goldenPath/help/gib.html README.md --template staticPage.html -t ../mirrorDocs/staticPage.lua
	chmod a+x $(DOCUMENTROOT)-beta/goldenPath/help/gbic.html
