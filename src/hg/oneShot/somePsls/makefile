include ../../../inc/makefile.h

L = $(MYSQLLIBS) -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = somePsls.o

somePsls: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/somePsls $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/somePsls
