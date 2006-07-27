include ../../inc/common.mk


L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = featureBits.o

featureBits: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/featureBits $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/featureBits${EXE}

compile:	$O
	${CC} ${COPT} ${CFLAGS} -o featureBits $O ${MYLIBS} $L

clean::
	rm -f featureBits ${O}
