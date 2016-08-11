kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = growNet.o

growNet: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/growNet $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/growNet${EXE}

clean:
	rm -f $O
