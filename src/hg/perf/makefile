include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = rangeQuery.o
A = rangeQuery

rangeQuery: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/${A} $O ${MYLIBS} $L

compile:	${O} ${MYLIBS}
	${CC} -o ${A} $O ${MYLIBS} $L


