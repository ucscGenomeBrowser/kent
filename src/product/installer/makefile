include ../../inc/common.mk

all:
	pandoc -t html5 -o $(DOCUMENTROOT)-$(USER)/goldenPath/help/gbic.html README.md --template staticPage.html -t ../markdown/staticPage.lua	
alpha:
	pandoc -t html5 -o $(DOCUMENTROOT)/goldenPath/help/gbic.html README.md --template staticPage.html -t ../markdown/staticPage.lua	
beta:
	pandoc -t html5 -o $(DOCUMENTROOT)-beta/goldenPath/help/gib.html README.md --template staticPage.html -t ../markdown/staticPage.lua	
