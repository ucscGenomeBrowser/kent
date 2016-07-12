kentSrc = ../../..
include ../../../inc/common.mk

MYLIBDIR = ${KENTROOT}/lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
L += ${MYSQLLIBS} -lm

A = tajD
O = ${A}.o

${A}:	${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${O} ${MYLIBS} ${L} -lm -g
	cp ${AOUT} ${BINDIR}/${A}
	mv ${AOUT} ${A}

tags:	
	ctags *.c *.h ../../lib/*.c ../../inc/*.h
	etags *.c *.h ../../lib/*.c ../../inc/*.h

clean:	
	rm ${O} ${BINDIR}/${A}
