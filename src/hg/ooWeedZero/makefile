include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooWeedZero.o

ooWeedZero: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/ooWeedZero $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/ooWeedZero
