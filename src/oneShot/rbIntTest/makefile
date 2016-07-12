kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = rbIntTest.o

rbIntTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/rbIntTest $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/rbIntTest${EXE}

clean:
	rm -f $O
