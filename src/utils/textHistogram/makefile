include ../../inc/makefile.h


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = textHistogram.o

textHistogram: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/textHistogram $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/textHistogram
