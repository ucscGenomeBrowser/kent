kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = plateOfPrimers.o

plateOfPrimers: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/plateOfPrimers $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/plateOfPrimers${EXE}

clean:
	rm -f $O
