include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgKnownToSuper.o

hgKnownToSuper: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgKnownToSuper $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgKnownToSuper${EXE}

clean::
	rm -f ${O}
