include ../../inc/common.mk

MYLIBDIR = ../../lib/${MACHTYPE}
L += ${MYSQLLIBS} -lm
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = genePredToGtf.o

genePredToGtf: $O ${MYLIBS} makefile
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/genePredToGtf${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/genePredToGtf${EXE}

test:
	cd tests && ${MAKE} test

compile: $O ${MYLIBS} makefile
	${CC} ${COPT} ${CFLAGS} -o genePredToGtf${EXE} $O ${MYLIBS} $L

clean:
	rm -f $O $B genePredToGtf
	cd tests && ${MAKE} clean
