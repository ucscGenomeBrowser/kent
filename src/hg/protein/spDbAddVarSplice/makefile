include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = spDbAddVarSplice.o

spDbAddVarSplice: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/spDbAddVarSplice $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/spDbAddVarSplice${EXE}

clean:
	rm -f $O
