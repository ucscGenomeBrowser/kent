include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = toDev64.o

toDev64: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/toDev64 $O ${MYLIBS} $L
	strip ${BINDIR}/toDev64

clean:
	rm -f $O
