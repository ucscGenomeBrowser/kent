include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = faToFastq.o

faToFastq: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/faToFastq $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/faToFastq${EXE}

clean:
	rm -f $O
