include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslCrudeCmp.o

pslCrudeCmp: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslCrudeCmp $O $(MYLIBS) $L
