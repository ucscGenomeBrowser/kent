include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txReadRa.o

txReadRa: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txReadRa $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txReadRa${EXE}

clean:
	rm -f $O
