.c.o:
	gcc -ggdb -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = htc.o

htc: lib $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc $O $(MYLIBS) $L
	mv a.out htc
	strip htc
	chmod a+rx htc
	mv htc ../../cgi-bin

lib:
	cd ../../lib; make
