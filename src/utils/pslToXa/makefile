include ../../inc/makefile.h


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslToXa.o

pslToXa: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslToXa $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/pslToXa
