include ../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = udcCleanup.o

udcCleanup: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/udcCleanup $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/udcCleanup${EXE}

clean:
	rm -f $O
