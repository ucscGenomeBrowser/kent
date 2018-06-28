kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = kgPepMrna.o

kgPepMrna: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/kgPepMrna $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/kgPepMrna${EXE}

clean::
	rm -f ${O}
