include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslToChain.o

pslToChain: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/pslToChain $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/pslToChain${EXE}

clean:
	rm -f $O
