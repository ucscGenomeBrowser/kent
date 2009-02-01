include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bigBedToBed.o

bigBedToBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bigBedToBed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bigBedToBed${EXE}

clean:
	rm -f $O
