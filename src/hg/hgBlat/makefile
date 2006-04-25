include ../../inc/common.mk

HG_WARN=-Wall

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

A = hgBlat

O = $A.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

clean::
	rm -f hgSs_*.fa hgSs_*.pslx

