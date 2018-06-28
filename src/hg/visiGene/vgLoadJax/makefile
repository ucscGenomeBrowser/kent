kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgLoadJax.o

vgLoadJax: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgLoadJax $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/vgLoadJax${EXE}

forwardSlices: forwardSlices.o ${MYLIBS}
	${CC} ${COPT} -o forwardSlices forwardSlices.o ${MYLIBS} $L

clean:
	rm -f $O
	rm -f forwardSlices forwardSlices.o
