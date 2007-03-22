include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafSpeciesList.o

mafSpeciesList: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mafSpeciesList $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/mafSpeciesList${EXE}

clean:
	rm -f $O
