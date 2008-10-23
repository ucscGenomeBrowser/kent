include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = shortRepeatMerge.o

shortRepeatMerge: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/shortRepeatMerge $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/shortRepeatMerge${EXE}

clean:
	rm -f $O
