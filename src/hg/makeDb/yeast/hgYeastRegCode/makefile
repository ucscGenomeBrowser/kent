include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgYeastRegCode.o

hgYeastRegCode: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgYeastRegCode $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/hgYeastRegCode

clean:
	rm -f $O
