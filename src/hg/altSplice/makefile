.c.o:
	gcc  -ggdb -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O =  altSplice.o 

TESTO = test.o

altSplice: $(O)
	gcc -o altSplice $O $(MYLIBS) $L
	cp altSplice $(HOME)/bin/$(MACHTYPE)/

test: $(TESTO)
	gcc -o test $(TESTO) $(MYLIBS) $L

lib:
	cd ../../lib; make
