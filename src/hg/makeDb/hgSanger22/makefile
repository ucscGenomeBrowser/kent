include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSanger22.o 

hgSanger22: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgSanger22 $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgSanger22
