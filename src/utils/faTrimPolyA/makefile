include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faTrimPolyA.o

faTrimPolyA: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faTrimPolyA $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/faTrimPolyA
