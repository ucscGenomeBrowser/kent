include ../../inc/common.mk

#	$Id: makefile,v 1.2 2006/05/22 22:55:37 hiram Exp $";

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = dbTrash.o

dbTrash: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/dbTrash $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/dbTrash

compile: $O ${MYLIBS}
	${CC} ${COPT} -o dbTrash $O ${MYLIBS} $L

clean:
	rm -f $O dbTrash
