include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafToAnc.o ancCheck.o

all: mafToAnc ancCheck

mafToAnc: mafToAnc.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafToAnc mafToAnc.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafToAnc${EXE}

ancCheck: ancCheck.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/ancCheck ancCheck.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/ancCheck${EXE}

clean:
	rm -f $O
