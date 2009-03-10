include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = mafSpeciesList.o

mafSpeciesList: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafSpeciesList $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafSpeciesList${EXE}

clean:
	rm -f $O
