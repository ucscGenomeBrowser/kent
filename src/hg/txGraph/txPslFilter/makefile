include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txPslFilter.o

txPslFilter: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txPslFilter $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txPslFilter${EXE}

clean:
	rm -f $O
