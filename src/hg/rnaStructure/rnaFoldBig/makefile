include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rnaFoldBig.o

rnaFoldBig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/rnaFoldBig $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/rnaFoldBig${EXE}

clean::
	rm -f ${O}
