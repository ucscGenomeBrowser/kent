include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtToMaf.o

axtToMaf: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtToMaf $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtToMaf${EXE}
