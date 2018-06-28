kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgPatchJax.o

vgPatchJax: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgPatchJax $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/vgPatchJax${EXE}

clean:
	rm -f $O
