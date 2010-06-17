include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = emblMatrixToMotif.o

emblMatrixToMotif: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/emblMatrixToMotif $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/emblMatrixToMotif${EXE}

clean::
	rm -f ${O}
