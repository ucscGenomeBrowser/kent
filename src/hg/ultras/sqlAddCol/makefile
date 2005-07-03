include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = sqlAddCol.o

sqlAddCol: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/sqlAddCol $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/sqlAddCol${EXE}

clean:
	rm -f $O
