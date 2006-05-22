include ../../inc/common.mk

#	$Id: makefile,v 1.1 2006/05/22 18:43:07 hiram Exp $";

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = dbTrash.o

dbTrash: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/dbTrash $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/dbTrash

clean:
	rm -f $O
