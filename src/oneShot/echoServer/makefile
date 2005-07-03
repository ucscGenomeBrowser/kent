include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = echoServer.o

echoServer: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/echoServer $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/echoServer${EXE}

clean:
	rm -f $O
