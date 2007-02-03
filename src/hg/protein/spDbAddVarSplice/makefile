include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = spDbAddVarSplice.o

spDbAddVarSplice: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/spDbAddVarSplice $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/spDbAddVarSplice${EXE}

clean:
	rm -f $O
