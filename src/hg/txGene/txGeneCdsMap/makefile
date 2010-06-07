include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneCdsMap.o

txGeneCdsMap: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneCdsMap $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneCdsMap${EXE}

clean:
	rm -f $O
