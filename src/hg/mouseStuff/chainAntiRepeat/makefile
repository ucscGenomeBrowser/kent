include ../../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = chainAntiRepeat.o

chainAntiRepeat: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/chainAntiRepeat $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/chainAntiRepeat${EXE}

clean:
	rm -f $O
