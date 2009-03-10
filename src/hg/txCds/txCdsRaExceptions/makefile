include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txCdsRaExceptions.o

txCdsRaExceptions: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsRaExceptions $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsRaExceptions${EXE}

clean:
	rm -f $O
