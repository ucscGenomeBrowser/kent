include ../../inc/common.mk

HG_WARN = ${HG_WARN_ERR}
L = -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = autoSql.o

autoSql: ${O}
	${CC} ${COPT} -o autoSql $O ../../lib/${MACHTYPE}/jkweb.a ${L}
	cp -f autoSql ${BINDIR}

test:
	cd tests && ${MAKE} test

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f autoSql ${O}
	cd tests && ${MAKE} clean

