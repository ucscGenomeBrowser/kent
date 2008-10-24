include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = splatMerge.o splatAli.o

splatMerge: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/splatMerge $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/splatMerge${EXE}

clean:
	rm -f $O
