include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = shuffleLines.o

shuffleLines: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/shuffleLines $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/shuffleLines

clean:
	rm -f $O
