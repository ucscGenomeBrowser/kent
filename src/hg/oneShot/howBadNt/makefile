.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = howBadNt.o

howBadNt: $O 
	gcc -o howBadNt $O $(MYLIBS) $L

