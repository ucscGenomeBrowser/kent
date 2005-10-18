include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = maxEachRow.o

maxEachRow: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/maxEachRow $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/maxEachRow

clean:
	rm -f $O
