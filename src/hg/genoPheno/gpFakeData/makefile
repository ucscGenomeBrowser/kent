include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gpFakeData.o

gpFakeData: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gpFakeData $O ${MYLIBS} $L
	strip ${BINDIR}/gpFakeData

clean:
	rm -f $O
