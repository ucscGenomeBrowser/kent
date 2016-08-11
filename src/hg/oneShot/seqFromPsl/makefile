kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = seqFromPsl.o

seqFromPsl: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/seqFromPsl $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/seqFromPsl${EXE}

clean:
	rm -f $O
