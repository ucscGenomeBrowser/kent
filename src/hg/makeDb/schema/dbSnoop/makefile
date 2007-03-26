include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = dbSnoop.o

dbSnoop: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/dbSnoop $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/dbSnoop${EXE}

clean:
	rm -f $O
