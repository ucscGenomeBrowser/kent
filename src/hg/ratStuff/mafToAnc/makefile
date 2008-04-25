include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafToAnc.o

mafToAnc: mafToAnc.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafToAnc mafToAnc.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafToAnc${EXE}

clean:
	rm -f $O

