kentSrc = ../../..
include ../../../inc/common.mk

MYLIBDIR = ${KENTROOT}/lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
L += ${MYSQLLIBS} -lm

A = dbSnp
O = ${A}.o

dbSnp:	${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${O} ${MYLIBS} ${L}
	mv ${AOUT} ${BINDIR}/${A}

tags:	
	ctags *.c *.h ../../lib/*.c ../../inc/*.h
	etags *.c *.h ../../lib/*.c ../../inc/*.h

clean:	
	rm ${O} ${BINDIR}/${A}
