include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = chainAntiRepeat.o

chainAntiRepeat: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/chainAntiRepeat $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/chainAntiRepeat

clean:
	rm -f $O
