.c.o:
	gcc -ggdb -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = coorXlat.o 

splitFa: $(O)
	gcc -o coorXlat $O $(MYLIBS) $L

