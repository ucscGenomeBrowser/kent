kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testIxx.o

testIxx: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/testIxx $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/testIxx${EXE}

clean:
	rm -f $O
