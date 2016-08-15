kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = pairCompress.o

pairCompress: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/pairCompress $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/pairCompress${EXE}

clean:
	rm -f $O
