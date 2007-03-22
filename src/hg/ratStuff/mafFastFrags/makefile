include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafFastFrags.o

mafFastFrags: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mafFastFrags $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/mafFastFrags${EXE}

clean:
	rm -f $O
