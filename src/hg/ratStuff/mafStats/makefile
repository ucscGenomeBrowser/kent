kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafStats.o

mafStats: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafStats $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafStats${EXE}

clean:
	rm -f $O
