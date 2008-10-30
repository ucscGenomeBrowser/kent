include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = itsaDump.o

itsaDump: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/itsaDump $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/itsaDump${EXE}

clean:
	rm -f $O
