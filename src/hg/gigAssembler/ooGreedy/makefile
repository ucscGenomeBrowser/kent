include ../../inc/makefile.h


L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooGreedy.o

ooGreedy: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/ooGreedy $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
