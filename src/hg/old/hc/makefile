.c.o:
	gcc -ggdb -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hc.o

hc: lib $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc $O $(MYLIBS) $L
	mv a.out hc
	strip hc
	chmod a+rx hc
	mv hc /usr/local/apache/cgi-bin

lib:
	cd ../lib; make
	cd ../../lib; make
