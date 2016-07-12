kentSrc = ../../..
include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafToClustalw.o

mafToClustalw: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafToClustalw $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafToClustalw${EXE}

clean:
	rm -f $O
