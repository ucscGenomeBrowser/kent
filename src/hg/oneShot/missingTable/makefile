.c.o:
	cc -O -I../../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = missingTable.o 

missingTable: $(O)
	cc -o missingTable $O $(MYLIBS) $L


