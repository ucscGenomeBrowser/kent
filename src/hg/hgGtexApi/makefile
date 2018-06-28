kentSrc = ../..
include ../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgGtexApi

O = $A.o

include ../../inc/cgi_build_rules.mk

compile: $O
	${CC} -o $A $O ${MYLIBS} ${L}
