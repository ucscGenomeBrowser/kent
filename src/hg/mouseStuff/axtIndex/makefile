include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtIndex.o

axtIndex: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtIndex $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtIndex${EXE}
