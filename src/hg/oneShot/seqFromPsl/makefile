include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = seqFromPsl.o

seqFromPsl: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/seqFromPsl $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/seqFromPsl

clean:
	rm -f $O
