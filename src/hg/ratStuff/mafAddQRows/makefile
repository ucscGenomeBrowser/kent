include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = $(MYLIBDIR)/jkhgap.a  ${MYLIBDIR}/jkweb.a

O = mafAddQRows.o

mafAddQRows: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafAddQRows $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafAddQRows${EXE}

clean:
	rm -f $O
