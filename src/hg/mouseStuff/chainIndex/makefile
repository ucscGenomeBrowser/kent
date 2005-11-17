include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = chainIndex.o

chainIndex: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/chainIndex $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/chainIndex

clean:
	rm -f $O
