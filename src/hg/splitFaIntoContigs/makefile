include ../../inc/common.mk


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitFaIntoContigs.o

splitFaIntoContigs: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/splitFaIntoContigs $O $(MYLIBS) $L
	cp $(HOME)/bin/$(MACHTYPE)/splitFaIntoContigs .

clean:
	rm -f *.o $(HOME)/bin/$(MACHTYPE)/splitFaIntoContigs ./splitFaIntoContigs
