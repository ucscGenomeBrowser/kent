include ../../../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgYeastRegCode.o

hgYeastRegCode: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgYeastRegCode $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgYeastRegCode${EXE}

clean:
	rm -f $O
