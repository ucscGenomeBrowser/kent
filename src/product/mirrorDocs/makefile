kentSrc = ../..
include ../../inc/common.mk

all:
	pandoc -t html5 -o $(DOCUMENTROOT)-$(USER)/admin/mirror.html ../README.txt --template staticPage.html -t staticPage.lua	
	for i in ../*.txt; do pandoc -t html5 -o $(DOCUMENTROOT)-$(USER)/admin/`basename $$i .txt`.html $$i --template staticPage.html -t staticPage.lua; done
alpha:
	pandoc -t html5 -o $(DOCUMENTROOT)/admin/mirror.html ../README.txt --template staticPage.html -t staticPage.lua	
	for i in ../*.txt; do pandoc -t html5 -o $(DOCUMENTROOT)/admin/`basename $$i .txt`.html  $$i --template staticPage.html -t staticPage.lua; done
beta:
	pandoc -t html5 -o $(DOCUMENTROOT)-beta/admin/mirror.html ../README.txt --template staticPage.html -t staticPage.lua	
	for i in ../*.txt; do pandoc -t html5 -o $(DOCUMENTROOT)-beta/admin/mirror.html ../README.First --template staticPage.html -t staticPage.lua; done
