.c.o:
	gcc -ggdb -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = test.o foo.o

test: lib $(O)
	gcc -o test $O $(MYLIBS) $L

lib:
	cd ../../lib;make
