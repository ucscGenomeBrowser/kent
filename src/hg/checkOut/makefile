.c.o:
	gcc -O -Wimplicit -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -I../inc -I../../inc -c $*.c

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkOut.o 

checkOut: $O $(MYLIBS)
	gcc -o checkOut $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
