include ../../inc/makefile.h


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooTester.o 

ooTester: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/ooTester $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
