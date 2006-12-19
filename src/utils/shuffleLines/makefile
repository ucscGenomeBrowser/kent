include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = shuffleLines.o

shuffleLines: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/shuffleLines $O ${MYLIBS} $L
	strip ${BINDIR}/shuffleLines

clean:
	rm -f $O
