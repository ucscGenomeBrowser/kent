include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = safePush.o

safePush: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/safePush $O ${MYLIBS} $L
	strip ${BINDIR}/safePush

clean:
	rm -f $O
