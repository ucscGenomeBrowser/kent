.c.o:
	gcc -O -I../inc -I../../inc -c $*.c

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgGenCon.o 

hgGenCon: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/hgGenCon $O ../../lib/$(MACHTYPE)/jkweb.a -lm

