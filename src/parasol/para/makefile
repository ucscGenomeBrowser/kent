.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = para.o jobDb.o jobResult.o

para: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/para $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/para
