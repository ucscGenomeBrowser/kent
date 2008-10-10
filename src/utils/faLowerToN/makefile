include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = faLowerToN.o

faLowerToN: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/faLowerToN $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/faLowerToN${EXE}

clean:
	rm -f $O
