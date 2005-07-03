include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = jccTest.o

jccTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/jccTest $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/jccTest${EXE}

clean:
	rm -f $O
