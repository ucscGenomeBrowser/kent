kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = wordChain.o

wordChain: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wordChain $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/wordChain${EXE}

clean:
	rm -f $O
