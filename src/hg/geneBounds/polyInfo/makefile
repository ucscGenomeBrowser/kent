.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../lib -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm  $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = polyInfo.o ../lib/estInfo.o

polyInfo: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/polyInfo $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/polyInfo
