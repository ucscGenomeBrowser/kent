kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = jccTest.o

jccTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/jccTest $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/jccTest${EXE}

clean:
	rm -f $O
