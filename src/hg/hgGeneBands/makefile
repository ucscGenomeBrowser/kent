kentSrc = ../..
include ../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

X = hgGeneBands
O = ${X}.o

${X}: $O ${MYLIBS}
	${CC} ${COPT} -o ${X} $O ${MYLIBS} $L

clean::
	rm -f ${O}
