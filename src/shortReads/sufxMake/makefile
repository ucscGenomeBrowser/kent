include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sufxMake.o

sufxMake: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sufxMake $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/sufxMake${EXE}

clean:
	rm -f $O
