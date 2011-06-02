include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = regClusterTreeCells
O = regClusterTreeCells.o

regClusterTreeCells: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}

test:
	regClusterTreeCells -short tests/in.lst tests/out.tree /dev/null
	diff tests/expected.tree tests/out.tree
