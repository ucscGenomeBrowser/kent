include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = minEachRow.o

minEachRow: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/minEachRow $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/minEachRow

clean:
	rm -f $O
