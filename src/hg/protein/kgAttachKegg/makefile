include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = kgAttachKegg.o

kgAttachKegg: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/kgAttachKegg $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/kgAttachKegg${EXE}

clean:
	rm -f $O
