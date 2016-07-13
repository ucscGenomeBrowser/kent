kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
CFLAGS= -ggdb

O = exonMap.o

exonMap: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/exonMap $O ${MYLIBS} $L

clean:
	rm -f $O
