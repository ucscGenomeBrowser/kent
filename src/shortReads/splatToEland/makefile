include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = splatToEland.o

splatToEland: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/splatToEland $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/splatToEland${EXE}

clean:
	rm -f $O
