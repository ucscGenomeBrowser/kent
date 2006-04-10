include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

X = dumpNib
O = ${X}.o

${X}: $O ${MYLIBS}
	${CC} ${COPT} -o ${X} $O ${MYLIBS} $L

X = countNib
O = ${X}.o

${X}: $O ${MYLIBS}
	${CC} ${COPT} -o ${X} $O ${MYLIBS} $L
