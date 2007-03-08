include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafSpeciesSubset.o

mafSpeciesSubset: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mafSpeciesSubset $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/mafSpeciesSubset${EXE}

clean:
	rm -f $O
