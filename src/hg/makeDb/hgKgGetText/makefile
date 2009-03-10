include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgKgGetText.o

hgKgGetText: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgKgGetText $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgKgGetText${EXE}

clean:
	rm -f $O
