kentSrc = ../..
include ../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgSession

O = $A.o backup.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} -o $A${EXE} -g $O ${MYLIBS} ${L} 

