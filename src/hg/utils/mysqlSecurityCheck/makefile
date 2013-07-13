include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mysqlSecurityCheck.o

mysqlSecurityCheck: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mysqlSecurityCheck $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mysqlSecurityCheck${EXE}

clean:
	rm -f $O
