include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = emblMatrixToMotif.o

emblMatrixToMotif: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/emblMatrixToMotif $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/emblMatrixToMotif
