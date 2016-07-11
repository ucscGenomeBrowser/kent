kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = chromPosLookup.o

chromPosLookup: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/chromPosLookup $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/chromPosLookup${EXE}

clean:
	rm -f $O
