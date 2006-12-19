include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = chainIndex.o

chainIndex: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/chainIndex $O ${MYLIBS} $L
	strip ${BINDIR}/chainIndex

clean:
	rm -f $O
