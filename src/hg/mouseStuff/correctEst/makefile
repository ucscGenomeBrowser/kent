include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = correctEst.o

correctEst: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/correctEst $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/correctEst${EXE}
