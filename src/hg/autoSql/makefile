include ../../inc/common.mk


L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = autoSql.o

autoSql: $(O)
	gcc -o autoSql $O ../../lib/$(MACHTYPE)/jkweb.a $(L)
	cp autoSql $(HOME)/bin/$(MACHTYPE)

test: test.o testHarness.o
	gcc -o test testHarness.o test.o $(MYLIBS) $(MYSQLLIBS)

newTest.c: newTest.as 
	autoSql newTest.as newTest -dbLink

dbLinkTest:  newTest.o dbLinkTest.o
	gcc -o dbLinkTest dbLinkTest.o newTest.o $(MYLIBS) $(MYSQLLIBS)

lib:
	cd ../../lib; make
