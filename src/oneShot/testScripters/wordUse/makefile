kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = wordUse.o

wordUse: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wordUse $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/wordUse${EXE}

clean:
	rm -f $O
