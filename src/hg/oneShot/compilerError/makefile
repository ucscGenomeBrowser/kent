kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = compilerError.o

compilerError: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/compilerError $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/compilerError${EXE}

clean:
	rm -f $O
