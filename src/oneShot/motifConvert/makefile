kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = motifConvert.o

motifConvert: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/motifConvert $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/motifConvert${EXE}

clean:
	rm -f $O
