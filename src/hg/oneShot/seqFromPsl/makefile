include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = seqFromPsl.o

seqFromPsl: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/seqFromPsl $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/seqFromPsl${EXE}

clean:
	rm -f $O
