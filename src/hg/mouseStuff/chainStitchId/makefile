include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = chainStitchId.o

chainStitchId: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/chainStitchId $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/chainStitchId${EXE}

clean:
	rm -f $O
