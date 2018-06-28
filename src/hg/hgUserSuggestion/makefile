kentSrc = ../..
include ../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a ../../htslib/libhts.a ${MYSQLLIBS}

A = hgUserSuggestion

O = $A.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} -o $A${EXE} $O ${MYLIBS} #${L}
