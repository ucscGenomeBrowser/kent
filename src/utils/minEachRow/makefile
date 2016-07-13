kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = minEachRow.o

minEachRow: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/minEachRow $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/minEachRow${EXE}

clean:
	rm -f $O
