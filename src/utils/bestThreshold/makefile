include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bestThreshold.o

bestThreshold: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/bestThreshold $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/bestThreshold${EXE}

test:
	bestThreshold in.good in.bad > out
	diff expected out

clean:
	rm -f $O
