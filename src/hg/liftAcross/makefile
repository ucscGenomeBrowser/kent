include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = liftAcross.o

liftAcross: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/liftAcross $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/liftAcross${EXE}

compile:	${O}
	${CC} ${COPT} -o liftAcross $O ${MYLIBS} $L

test:
	(cd tests && ${MAKE})

clean:
	rm -f $O liftAcross
	(cd tests && ${MAKE} clean)
