include ../../inc/common.mk

HG_WARN = ${HG_WARN_ERR}
L += -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a


O = autoSql.o

all: ${BINDIR}/autoSql

${BINDIR}/autoSql: autoSql
	rm -f $@
	cp -p $< $@

autoSql: ${O}
	${CC} ${COPT} -o autoSql $O ../../lib/${MACHTYPE}/jkweb.a ${L}

test:
	cd tests && ${MAKE} test

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f autoSql ${O}
	cd tests && ${MAKE} clean

