include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = wordUse.o

wordUse: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/wordUse $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/wordUse${EXE}

clean:
	rm -f $O
