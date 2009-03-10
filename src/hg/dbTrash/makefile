include ../../inc/common.mk

#	$Id: makefile,v 1.5 2009/03/10 00:33:46 galt Exp $";

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = dbTrash.o

dbTrash: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/dbTrash $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/dbTrash${EXE}

compile: $O ${MYLIBS}
	${CC} ${COPT} -o dbTrash $O ${MYLIBS} $L

clean:
	rm -f $O dbTrash
