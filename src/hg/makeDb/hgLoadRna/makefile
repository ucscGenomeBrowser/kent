F64 = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
.c.o:
	gcc -ggdb ${F64} -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadRna.o 

hgLoadRna: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb ${F64} -o $(HOME)/bin/$(MACHTYPE)/hgLoadRna $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
