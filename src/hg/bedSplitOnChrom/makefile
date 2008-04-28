include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bedSplitOnChrom.o

bedSplitOnChrom: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bedSplitOnChrom $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bedSplitOnChrom${EXE}

clean:
	rm -f $O
