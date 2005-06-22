include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = raSort.o

raSort: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/raSort $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/raSort

clean:
	rm -f $O
