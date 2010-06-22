include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgCeOrfToGene.o

hgCeOrfToGene: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgCeOrfToGene $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgCeOrfToGene${EXE}

clean::
	rm -f ${O}
