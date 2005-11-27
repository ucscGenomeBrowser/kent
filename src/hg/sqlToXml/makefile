include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = sqlToXml.o

sqlToXml: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/sqlToXml $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/sqlToXml

clean:
	rm -f $O
