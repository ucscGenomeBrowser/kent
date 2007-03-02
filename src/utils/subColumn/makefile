include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = subColumn.o

subColumn: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/subColumn $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/subColumn${EXE}

clean:
	rm -f $O
