include ../../inc/makefile.h


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = textHist2.o

textHist2: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/textHist2 $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/textHist2
