include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testIxx.o

testIxx: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/testIxx $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/testIxx

clean:
	rm -f $O
