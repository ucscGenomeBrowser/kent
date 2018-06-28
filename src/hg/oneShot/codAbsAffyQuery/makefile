kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = codAbsAffyQuery.o

codAbsAffyQuery: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/codAbsAffyQuery $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/codAbsAffyQuery${EXE}

clean:
	rm -f $O
