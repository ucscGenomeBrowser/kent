.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkPsl.o 

checkPsl: $O $(MYLIBS)
	gcc -ggdb -o checkPsl $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
