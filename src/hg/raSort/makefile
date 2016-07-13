kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = raSort.o

raSort: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/raSort $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/raSort${EXE}

clean:
	rm -f $O
