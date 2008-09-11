include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = dbSnoop.o

dbSnoop: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/dbSnoop $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/dbSnoop${EXE}

compile: $O ${MYLIBS}
	${CC} ${COPT} -o dbSnoop $O ${MYLIBS} $L

clean:
	rm -f $O dbSnoop
