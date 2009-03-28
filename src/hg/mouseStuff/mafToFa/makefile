include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafToFa.o

mafToFa: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafToFa $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafToFa${EXE}

clean:
	rm -f $O
