include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = pairCompress.o

pairCompress: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pairCompress $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/pairCompress${EXE}

clean:
	rm -f $O
