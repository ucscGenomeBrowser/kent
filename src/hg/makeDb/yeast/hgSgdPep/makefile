include ../../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSgdPep.o

hgSgdPep: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgSgdPep $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgSgdPep
