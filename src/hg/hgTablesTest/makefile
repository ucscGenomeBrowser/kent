include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgTablesTest.o

hgTablesTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgTablesTest $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/hgTablesTest

clean:
	rm -f $O
