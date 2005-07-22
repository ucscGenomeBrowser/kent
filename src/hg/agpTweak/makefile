include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = agpTweak
O = ${A}.o

agpToFa: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/${A} \
		$O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/${A}${EXE}

compile: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A} $O ${MYLIBS} $L

clean::
	rm -f ${A} ${A}.o
