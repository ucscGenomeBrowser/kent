include ../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = blastToPsl.o

blastToPsl: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/blastToPsl $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/blastToPsl
