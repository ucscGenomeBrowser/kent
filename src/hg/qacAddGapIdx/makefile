kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = qacAddGapIdx.o

qacAddGapIdx: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/qacAddGapIdx $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/qacAddGapIdx${EXE}

clean:
	rm -f $O
