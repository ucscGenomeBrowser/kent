include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = agpToFa.o

agpToFa: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/agpToFa \
		$O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/agpToFa${EXE}

compile: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o agpToFa $O ${MYLIBS} $L

clean::
	rm -f agpToFa.o agpToFa
