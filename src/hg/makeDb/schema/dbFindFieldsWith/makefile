include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = dbFindFieldsWith.o

dbFindFieldsWith: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/dbFindFieldsWith $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/dbFindFieldsWith${EXE}

clean:
	rm -f $O
