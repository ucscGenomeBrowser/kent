include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = averagExp.o

averagExp: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/averagExp $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/averagExp${EXE}
