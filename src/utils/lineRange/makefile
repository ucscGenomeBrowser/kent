include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = lineRange.o

lineRange: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/lineRange $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/lineRange${EXE}

clean:
	rm -f $O
