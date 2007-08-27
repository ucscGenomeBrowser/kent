include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = affyVsIlluminaCommon.o

affyVsIlluminaCommon: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/affyVsIlluminaCommon $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/affyVsIlluminaCommon${EXE}

clean:
	rm -f $O
