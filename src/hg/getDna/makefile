.c.o:
	gcc -O -I../inc -I../../inc -c $*.c

L =
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = getDna.o 

getDna: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/getDna $O ../../lib/$(MACHTYPE)/jkweb.a

