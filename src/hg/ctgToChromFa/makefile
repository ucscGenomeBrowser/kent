.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = ctgToChromFa.o 

ctgToChromFa: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/ctgToChromFa $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/ctgToChromFa

lib:
	cd ../../lib; make
