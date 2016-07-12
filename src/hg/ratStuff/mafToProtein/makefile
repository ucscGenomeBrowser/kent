kentSrc = ../../..
include ../../../inc/common.mk

CFLAGS= -ggdb

L +=  ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a   ${MYLIBDIR}/jkweb.a

O = mafToProtein.o

mafToProtein: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafToProtein $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/mafToProtein${EXE}

clean:
	rm -f $O
