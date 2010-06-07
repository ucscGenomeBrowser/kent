include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bestThreshold.o

bestThreshold: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bestThreshold $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bestThreshold${EXE}

test:
	bestThreshold in.good in.bad > out
	diff expected out

clean:
	rm -f $O
