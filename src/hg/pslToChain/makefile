include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslToChain.o

pslToChain: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslToChain $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/pslToChain${EXE}

clean:
	rm -f $O
