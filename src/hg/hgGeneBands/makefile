include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

X = hgGeneBands
O = ${X}.o

${X}: $O ${MYLIBS}
	${CC} ${COPT} -o ${X} $O ${MYLIBS} $L
