include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = dnaMotifFind.o 

dnaMotifFind: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/dnaMotifFind $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/dnaMotifFind
