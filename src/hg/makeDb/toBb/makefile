include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = toBb.o

toBb: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/toBb $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/toBb
