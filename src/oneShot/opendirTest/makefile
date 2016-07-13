kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = opendirTest.o

opendirTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/opendirTest $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/opendirTest${EXE}

clean:
	rm -f $O
