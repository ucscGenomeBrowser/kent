kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = jingTestFoo.o

jingTestFoo: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/jingTestFoo $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/jingTestFoo${EXE}

clean:
	rm -f $O
