kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = bedToBinDex.o

bedToBinDex: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bedToBinDex $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bedToBinDex${EXE}

clean:
	rm -f $O
