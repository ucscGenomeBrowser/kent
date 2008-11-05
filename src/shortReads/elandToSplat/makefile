include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = elandToSplat.o

elandToSplat: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/elandToSplat $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/elandToSplat${EXE}

clean:
	rm -f $O
