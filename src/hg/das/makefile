include ../../inc/common.mk

HG_WARN=-Wall

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = das

O = $A.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} ${COPT} ${CFLAGS} ${HG_WARN} $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}
	chmod a+rx $A
