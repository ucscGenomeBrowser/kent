kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = faToMaf.o

faToMaf: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/faToMaf $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/faToMaf${EXE}

clean:
	rm -f $O
