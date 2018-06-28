kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = gsidPosSel.o

gsidPosSel: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gsidPosSel $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/gsidPosSel${EXE}

clean:
	rm -f $O
