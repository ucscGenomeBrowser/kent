include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = greedyPack.o

greedyPack: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/greedyPack $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/greedyPack${EXE}

clean:
	rm -f $O
