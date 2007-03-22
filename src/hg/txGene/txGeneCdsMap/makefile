include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneCdsMap.o

txGeneCdsMap: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txGeneCdsMap $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txGeneCdsMap${EXE}

clean:
	rm -f $O
