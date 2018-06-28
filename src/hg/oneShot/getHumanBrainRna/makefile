kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = getHumanBrainRna.o

getHumanBrainRna: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/getHumanBrainRna $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/getHumanBrainRna${EXE}

clean:
	rm -f $O
