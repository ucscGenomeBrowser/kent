include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitFaIntoContigs.o

splitFaIntoContigs: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/splitFaIntoContigs $O ${MYLIBS} ${L}

clean:
	rm -f *.o $(HOME)/bin/$(MACHTYPE)/splitFaIntoContigs ./splitFaIntoContigs
