include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = headRest.o

headRest: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/headRest $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/headRest${EXE}

clean:
	rm -f $O
