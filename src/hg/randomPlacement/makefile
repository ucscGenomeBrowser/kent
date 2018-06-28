kentSrc = ../..
include ../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = randomPlacement

O = randomPlacement.o

${A}: ${O} $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A}${EXE} \
		$O ${MYLIBS} ${L} 
	${STRIP} ${BINDIR}/${A}${EXE}

compile: $O 
	${CC} -g $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

clean::
	rm -f ${O} ${A}${EXE} ${AOUT}
