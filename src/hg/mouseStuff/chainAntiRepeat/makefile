include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = chainAntiRepeat.o

chainAntiRepeat: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/chainAntiRepeat $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/chainAntiRepeat${EXE}

clean:
	rm -f $O
