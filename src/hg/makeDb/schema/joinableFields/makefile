include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = joinableFields.o

joinableFields: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/joinableFields $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/joinableFields

clean:
	rm -f $O
