include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgMapToGene.o

hgMapToGene: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgMapToGene $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/hgMapToGene

clean::
	rm -f ${O} hgMapToGene
