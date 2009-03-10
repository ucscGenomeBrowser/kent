include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = safePush.o

safePush: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/safePush $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/safePush${EXE}

clean:
	rm -f $O
