include ../../inc/makefile.h


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gapper.o 

gapper: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gapper $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
