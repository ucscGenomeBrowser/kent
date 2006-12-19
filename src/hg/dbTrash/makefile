include ../../inc/common.mk

#	$Id: makefile,v 1.3 2006/12/19 23:34:57 angie Exp $";

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = dbTrash.o

dbTrash: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/dbTrash $O ${MYLIBS} $L
	strip ${BINDIR}/dbTrash

compile: $O ${MYLIBS}
	${CC} ${COPT} -o dbTrash $O ${MYLIBS} $L

clean:
	rm -f $O dbTrash
