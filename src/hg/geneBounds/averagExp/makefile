include ../../../inc/makefile.h

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = averagExp.o

averagExp: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/averagExp $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/averagExp
