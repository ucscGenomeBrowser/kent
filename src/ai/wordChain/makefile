include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = wordChain.o

wordChain: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/wordChain $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/wordChain${EXE}

clean:
	rm -f $O
