include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = rbIntTest.o

rbIntTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/rbIntTest $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/rbIntTest

clean:
	rm -f $O
