kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = rt1dTest.o

rt1dTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/rt1dTest $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/rt1dTest${EXE}

clean:
	rm -f $O
