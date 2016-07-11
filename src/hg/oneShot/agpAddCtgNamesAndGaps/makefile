kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = agpAddCtgNamesAndGaps.o

agpAddCtgNamesAndGaps: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/agpAddCtgNamesAndGaps $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/agpAddCtgNamesAndGaps${EXE}

clean:
	rm -f $O
