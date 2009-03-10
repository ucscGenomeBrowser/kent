include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = toDev64.o

toDev64: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/toDev64 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/toDev64${EXE}

clean:
	rm -f $O
