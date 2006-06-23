include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = opendirTest.o

opendirTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/opendirTest $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/opendirTest${EXE}

clean:
	rm -f $O
