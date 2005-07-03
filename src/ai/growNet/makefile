include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = growNet.o

growNet: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/growNet $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/growNet${EXE}

clean:
	rm -f $O
