include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = emblMatrixToMotif.o

emblMatrixToMotif: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/emblMatrixToMotif $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/emblMatrixToMotif${EXE}
