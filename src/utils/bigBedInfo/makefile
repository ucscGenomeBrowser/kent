include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bigBedInfo.o

bigBedInfo: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bigBedInfo $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bigBedInfo${EXE}

clean:
	rm -f $O
