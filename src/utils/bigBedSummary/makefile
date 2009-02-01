include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bigBedSummary.o

bigBedSummary: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bigBedSummary $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bigBedSummary${EXE}

clean:
	rm -f $O
