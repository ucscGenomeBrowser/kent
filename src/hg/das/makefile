kentSrc = ../..
include ../../inc/common.mk

HG_WARN=-Wall

#L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ../../htslib/libhts.a ${MYSQLLIBS}

A = das

O = $A.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} ${COPT} ${CFLAGS} ${HG_WARN} $O  ${MYLIBS}
	mv ${AOUT} $A${EXE}
	chmod a+rx $A
