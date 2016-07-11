kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = afsCombine.o

afsCombine: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/afsCombine $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/afsCombine${EXE}

clean:
	rm -f $O
