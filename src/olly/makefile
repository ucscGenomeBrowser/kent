include ../inc/common.mk

L = -lm
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = olly.o

olly: $O $(MYLIBS)
#	gcc -ggdb -o olly $O $(MYLIBS) $L
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/olly $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/olly${EXE}
