include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chimpSuperQuals.o

chimpSuperQuals: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/chimpSuperQuals $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/chimpSuperQuals
