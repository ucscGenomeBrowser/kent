include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = echoServer.o

echoServer: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/echoServer $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/echoServer

clean:
	rm -f $O
