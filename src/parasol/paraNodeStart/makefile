.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = paraNodeStart.o

paraNodeStart: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraNodeStart $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/paraNodeStart
