kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = sqlAddCol.o

sqlAddCol: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sqlAddCol $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/sqlAddCol${EXE}

clean:
	rm -f $O
