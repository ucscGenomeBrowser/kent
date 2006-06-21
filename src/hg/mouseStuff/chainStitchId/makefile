include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = chainStitchId.o

chainStitchId: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/chainStitchId $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/chainStitchId${EXE}

clean:
	rm -f $O
