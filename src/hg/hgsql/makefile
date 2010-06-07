include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgsql.o

hgsql: $O ${MYLIBS}
	${CC} -o ${BINDIR}/hgsql $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgsql${EXE}

clean::
	rm -f ${O}
