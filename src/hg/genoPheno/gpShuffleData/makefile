include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gpShuffleData.o

gpShuffleData: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gpShuffleData $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/gpShuffleData

clean:
	rm -f $O
