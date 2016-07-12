kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = fakeLinkage.o

fakeLinkage: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/fakeLinkage $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/fakeLinkage${EXE}

clean:
	rm -f $O
