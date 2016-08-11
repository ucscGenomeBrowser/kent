kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = aliFragScore.o

aliFragScore: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/aliFragScore $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/aliFragScore${EXE}

clean:
	rm -f $O
