include ../../inc/common.mk


L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = featureBits.o

featureBits: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/featureBits $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/featureBits${EXE}

compile:	$O
	${CC} ${COPT} ${CFLAGS} -o featureBits $O ${MYLIBS} $L

clean::
	rm -f featureBits ${O}
