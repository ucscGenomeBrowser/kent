kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = regionToPrimer3.o

regionToPrimer3: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/regionToPrimer3 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/regionToPrimer3${EXE}

clean:
	rm -f $O
