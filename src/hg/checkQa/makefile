.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkQa.o 

checkQa: $O $(MYLIBS)
	gcc -ggdb -o checkQa $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
