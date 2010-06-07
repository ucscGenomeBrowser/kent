include ../../inc/common.mk


L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = getFeatDna.o

getFeatDna: $O ${MYLIBS}
	${CC} -o ${BINDIR}/getFeatDna $O ${MYLIBS} $L

clean::
	rm -f ${O}
