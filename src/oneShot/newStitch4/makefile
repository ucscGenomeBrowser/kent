include ../../inc/makefile.h


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newStitch4.o 

newStitch4: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/newStitch4 $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/newStitch4
