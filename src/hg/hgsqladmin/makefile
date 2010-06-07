include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgsqladmin.o

hgsqladmin: $O ${MYLIBS}
	${CC} -o ${BINDIR}/hgsqladmin $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgsqladmin${EXE}

clean::
	rm -f ${O}
