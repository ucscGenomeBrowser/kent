include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSgdGff3.o

hgSgdGff3: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgSgdGff3 $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgSgdGff3
