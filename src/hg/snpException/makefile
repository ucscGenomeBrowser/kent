include ../../inc/common.mk

MYLIBDIR = ${KENTROOT}/lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
L = ${MYSQLLIBS} -lm
HG_WARN = -Wall

A = snpException
O = ${A}.o

${A}:	${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${O} ${MYLIBS} ${L}
	mv a.out ${HOME}/bin/${MACHTYPE}/${A}

clean:	
	rm ${O} ${HOME}/bin/${MACHTYPE}/${A}
