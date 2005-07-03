include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = raSort.o

raSort: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/raSort $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/raSort${EXE}

clean:
	rm -f $O
