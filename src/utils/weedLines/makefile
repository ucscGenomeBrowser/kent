include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = weedLines.o

weedLines: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/weedLines $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/weedLines${EXE}

clean:
	rm -f $O
