include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txCdsOrfInfo.o

txCdsOrfInfo: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsOrfInfo $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsOrfInfo${EXE}

clean:
	rm -f $O
