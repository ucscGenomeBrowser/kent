kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = echoServer.o

echoServer: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/echoServer $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/echoServer${EXE}

clean:
	rm -f $O
