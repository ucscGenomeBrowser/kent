kentSrc = ../..
include ../../inc/common.mk

HG_WARN=-Wall

L += ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

A = hgPcr

O = $A.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}
