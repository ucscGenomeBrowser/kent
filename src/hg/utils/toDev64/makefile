include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = toDev64.o

toDev64: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/toDev64 $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/toDev64

clean:
	rm -f $O
