include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgPhMouse.o

hgPhMouse: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgPhMouse $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgPhMouse
