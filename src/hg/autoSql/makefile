include ../../inc/common.mk

HG_WARN = ${HG_WARN_ERR}
L = -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = autoSql.o

autoSql: ${O}
	${CC} -o autoSql $O ../../lib/${MACHTYPE}/jkweb.a ${L}
	cp autoSql ${HOME}/bin/${MACHTYPE}

test:
	cd tests && ${MAKE} test

lib:
	cd ../../lib; make

clean::
	rm -f autoSql ${O}
	cd tests && ${MAKE} clean

