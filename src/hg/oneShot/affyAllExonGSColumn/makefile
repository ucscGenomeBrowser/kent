kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = affyAllExonGSColumn.o

affyAllExonGSColumn: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/affyAllExonGSColumn $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/affyAllExonGSColumn${EXE}

clean:
	rm -f $O
