include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 


O = txGeneSeparateNoncoding.o

txGeneSeparateNoncoding: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txGeneSeparateNoncoding $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txGeneSeparateNoncoding${EXE}

clean:
	rm -f $O
