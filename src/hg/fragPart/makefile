include ../../inc/makefile.h


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = fragPart.o 

fragPart: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/fragPart $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/fragPart

lib:
	cd ../../lib; make
