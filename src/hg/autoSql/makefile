.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -I../inc -I../../inc -c $*.c

L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = autoSql.o

autoSql: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/autoSql $O ../../lib/$(MACHTYPE)/jkweb.a $(L)
	strip $(HOME)/bin/$(MACHTYPE)/autoSql

test: test.o testHarness.o
	gcc -o test testHarness.o test.o $(MYLIBS) $(MYSQLLIBS)

lib:
	cd ../../lib; make
