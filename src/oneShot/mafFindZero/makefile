include ../../inc/makefile.h


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafFindZero.o

mafFindZero: $O $(MYLIBS)
	gcc -ggdb -o mafFindZero $O $(MYLIBS) $L
	strip mafFindZero
