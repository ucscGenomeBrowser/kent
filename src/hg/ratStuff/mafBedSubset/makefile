kentSrc = ../../..
include ../../../inc/common.mk

CFLAGS= -ggdb
L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = mafBedSubset
O = ${A}.o

${A}: compile
	${STRIP} $(BINDIR)/${A}${EXE}

compile: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O ${MYLIBS} ${L}

clean::
	rm -f ${O}
