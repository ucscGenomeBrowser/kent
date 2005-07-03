include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = countLines.o

countLines: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/countLines $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/countLines${EXE}

clean:
	rm -f $O
