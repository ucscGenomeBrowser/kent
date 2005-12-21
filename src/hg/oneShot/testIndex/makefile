include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testIndex.o

testIndex: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/testIndex $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/testIndex

clean:
	rm -f $O
