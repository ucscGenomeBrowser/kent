include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = twinOrf2.o

twinOrf2: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/twinOrf2 $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/twinOrf2${EXE}
