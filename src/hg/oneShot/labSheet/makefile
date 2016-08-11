kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = labSheet.o

labSheet: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/labSheet $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/labSheet${EXE}

clean:
	rm -f $O
