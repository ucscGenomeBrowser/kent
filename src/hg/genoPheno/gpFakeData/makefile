include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gpFakeData.o

gpFakeData: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gpFakeData $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/gpFakeData

clean:
	rm -f $O
