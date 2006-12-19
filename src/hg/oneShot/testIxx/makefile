include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testIxx.o

testIxx: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/testIxx $O ${MYLIBS} $L
	strip ${BINDIR}/testIxx

clean:
	rm -f $O
