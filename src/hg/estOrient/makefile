include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


A = estOrient
O = estOrient.o

estOrient: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $A estOrient
	cd tests && ${MAKE} clean
