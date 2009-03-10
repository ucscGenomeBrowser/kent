include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 


O = txGeneSeparateNoncoding.o

txGeneSeparateNoncoding: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneSeparateNoncoding $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneSeparateNoncoding${EXE}

clean:
	rm -f $O
