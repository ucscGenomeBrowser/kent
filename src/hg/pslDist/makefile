kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
CFLAGS= -ggdb

O = pslDist.o

pslDist: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/pslDist $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/pslDist${EXE}

clean:
	rm -f $O
