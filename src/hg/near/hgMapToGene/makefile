include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgMapToGene.o

hgMapToGene: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgMapToGene $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgMapToGene${EXE}

clean::
	rm -f ${O} hgMapToGene
