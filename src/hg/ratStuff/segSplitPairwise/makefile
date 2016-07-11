kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = segSplitPairwise.o

segSplitPairwise: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/segSplitPairwise $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/segSplitPairwise${EXE}

clean:
	rm -f $O
