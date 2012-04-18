include ../../inc/common.mk

L += 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

INSTALLDIR = ${BINDIR}

PROG = scaffoldFaToAgp

O = ${PROG}.o 

${PROG}: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${INSTALLDIR}/${PROG} $O ${MYLIBS} ${L}

test: ${INSTALLDIR}/${PROG}
	cd test && csh runtest

clean: 
	rm -f ${O}
