kentSrc = ../../../..
include ../../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = reTraceFixMaf.o

reTraceFixMaf: $O ${MYLIBS}
	${CC} ${COPT} -ggdb -o ${BINDIR}/reTraceFixMaf $O ${MYLIBS} $L

clean:
	rm -f $O
