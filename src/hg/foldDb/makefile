include ../../inc/makefile.h


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = foldDb.o

foldDb: $O $(MYLIBS)
	gcc -ggdb -o foldDb $O $(MYLIBS) $L

lib:
	cd ../lib; make
	cd ../../lib; make
