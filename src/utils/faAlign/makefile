include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = faAlign.o

faAlign: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/faAlign $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/faAlign${EXE}

clean:
	rm -f $O
