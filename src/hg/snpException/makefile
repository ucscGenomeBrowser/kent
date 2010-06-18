include ../../inc/common.mk

MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
L += ${MYSQLLIBS} -lm

A = snpException
O = ${A}.o

${A}:	${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${O} ${MYLIBS} ${L}
	mv ${AOUT} ${BINDIR}/${A}

clean::
	rm -f ${O}
