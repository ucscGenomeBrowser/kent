.c.o:
	gcc -O -ggdb -I/cse/guests/kent/src/inc -I/cse/guests/kent/src/hg/inc -c $*.c

O = geneStarts.o 

L = $(MYSQLLIBS) -lm 
MYLIBDIR = /cse/guests/kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


geneStarts: $(O)
	gcc -o geneStarts $O $(MYLIBS) $L

