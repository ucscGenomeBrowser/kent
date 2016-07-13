kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = letterChain.o

letterChain: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/letterChain $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/letterChain${EXE}

clean:
	rm -f $O
