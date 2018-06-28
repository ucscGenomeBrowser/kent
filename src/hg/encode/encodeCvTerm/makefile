kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

A = encodeCvTerm
O = $A.o
    
compile: $O
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

install: $A${EXE}
	cp $A${EXE} ${BINDIR}
