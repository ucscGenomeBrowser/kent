include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafSpeciesSubset.o

mafSpeciesSubset: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafSpeciesSubset $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafSpeciesSubset${EXE}

clean:
	rm -f $O
