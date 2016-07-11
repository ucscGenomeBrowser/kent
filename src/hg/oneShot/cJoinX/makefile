kentSrc = ../../..
include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = cJoinX.o

cJoinX: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/cJoinX $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/cJoinX${EXE}

clean:
	rm -f $O
