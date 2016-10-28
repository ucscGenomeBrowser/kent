kentSrc = ../..
include ../../inc/common.mk

HG_WARN=-Wall

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgGtexTrackSettings

O = $A.o

include ../../inc/cgi_build_rules.mk

${A}.o: ${A}.c

compile:: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
