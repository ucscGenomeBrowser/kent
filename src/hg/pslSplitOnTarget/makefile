include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslSplitOnTarget.o

pslSplitOnTarget: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/pslSplitOnTarget $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/pslSplitOnTarget${EXE}

clean:
	rm -f $O
