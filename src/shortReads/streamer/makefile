include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = streamer.o

streamer: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/streamer $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/streamer${EXE}

clean:
	rm -f $O
