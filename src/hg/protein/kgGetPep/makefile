kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = kgGetPep
O = $A.o

$A: $O ${MYLIBS}
	${CC} ${COPT} ${HG_WARN} ${HG_DEFS} \
		-o ${BINDIR}/$A $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/$A${EXE}

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
	chmod a+rx $A

clean::
	rm -f ${O} ${A}
