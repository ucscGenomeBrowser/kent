include ../../inc/common.mk

L += ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

A = hgUserSuggestion

O = $A.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} -o $A${EXE} -g $O ${MYLIBS} ${L} 

