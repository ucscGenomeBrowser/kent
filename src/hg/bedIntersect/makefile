include ../../inc/makefile.h


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedIntersect.o

bedIntersect: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/bedIntersect $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/bedIntersect
