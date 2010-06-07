include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = agpToFa.o

agpToFa: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/agpToFa \
		$O ${MYLIBS} $L
	${STRIP} ${BINDIR}/agpToFa${EXE}

compile: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o agpToFa $O ${MYLIBS} $L

clean::
	rm -f agpToFa.o agpToFa
