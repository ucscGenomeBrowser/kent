include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgsqlLocal.o

hgsql: $O ${MYLIBS}
	${CC} -o ${BINDIR}/hgsqlLocal $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgsqlLocal${EXE}

clean::
	rm -f ${O}
