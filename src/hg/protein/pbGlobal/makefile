include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkpbap.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = pbGlobal

O = $A.o

include ../../../inc/cgi_build_rules.mk

lib::
	cd ../../../lib; make

compile: $O 
	${CC} -o $A $O ${MYLIBS} ${L}

mgsc: strip
	mv $A ${CGI_BIN}/$A

