.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStatus.o

paraNodeStatus: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraNodeStatus $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/paraNodeStatus

paraStat: paraStat.o
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraStat paraStat.o ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/paraStat
