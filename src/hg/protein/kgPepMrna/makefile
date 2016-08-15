kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = kgPepMrna.o

kgPepMrna: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/kgPepMrna $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/kgPepMrna${EXE}

clean::
	rm -f ${O}
