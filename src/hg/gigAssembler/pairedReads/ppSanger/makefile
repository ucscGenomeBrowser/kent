.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ppSanger.o sangInsert.o sangRange.o sangRead.o

ppSanger: $O $(MYLIBS)
	gcc -ggdb -o ppSanger $O $(MYLIBS) $L
	mv ppSanger $(HOME)/bin/$(MACHTYPE)


lib:
	cd ../lib; make
	cd ../../lib; make
