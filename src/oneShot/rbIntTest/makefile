include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = rbIntTest.o

rbIntTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/rbIntTest $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/rbIntTest${EXE}

clean:
	rm -f $O
