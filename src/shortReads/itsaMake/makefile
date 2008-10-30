include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = itsaMake.o

itsaMake: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/itsaMake $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/itsaMake${EXE}

clean:
	rm -f $O
