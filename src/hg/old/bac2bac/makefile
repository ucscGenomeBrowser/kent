.c.o:
	gcc -ggdb -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = bac2bac.o

bac2bac: $(O)
	gcc -o bac2bac $O ../../lib/$(MACHTYPE)/jkweb.a $(L)
	cp bac2bac ../../cgi-bin
	strip ../../cgi-bin/bac2bac

