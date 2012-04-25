include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgEncodeApi

O = $A.o

include ../../../inc/cgi_build_rules.mk

compile: $O
	${CC} -o $A $O ${MYLIBS} ${L}
