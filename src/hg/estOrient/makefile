include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = estOrient.o
B =  ${BINDIR}/estOrient

$B: estOrient
	@${MKDIR} `dirname $B`
	cp -f estOrient $B

estOrient: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o estOrient $O ${MYLIBS} ${MYSQLLIBS} $L

estOrient.o: estOrient.c $H

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B estOrient
	cd tests && ${MAKE} clean

