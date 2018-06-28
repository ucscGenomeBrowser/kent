kentSrc = ../..
include ../../inc/common.mk

#L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a ${MYSQLLIBS} ../../htslib/libhts.a

A = hgBlat

O = $A.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

clean::
	rm -f hgSs_*.fa hgSs_*.pslx
