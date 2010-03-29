include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = mafsInRegion
O = ${A}.o

${A}: compile
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A} $O ${MYLIBS} ${L}

clean:
	rm -f $O
