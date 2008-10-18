include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = splixMake.o

splixMake: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/splixMake $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/splixMake${EXE}

clean:
	rm -f $O
