include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sufxFind.o

sufxFind: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sufxFind $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/sufxFind${EXE}

clean:
	rm -f $O
