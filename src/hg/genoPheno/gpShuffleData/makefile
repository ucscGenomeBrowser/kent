include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gpShuffleData.o

gpShuffleData: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gpShuffleData $O ${MYLIBS} $L
	strip ${BINDIR}/gpShuffleData

clean:
	rm -f $O
