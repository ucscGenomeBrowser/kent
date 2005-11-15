include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = dbSnoop.o

dbSnoop: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/dbSnoop $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/dbSnoop

clean:
	rm -f $O
