include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgCeOrfToGene.o

hgCeOrfToGene: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgCeOrfToGene $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgCeOrfToGene${EXE}
