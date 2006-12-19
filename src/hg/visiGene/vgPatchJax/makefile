include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgPatchJax.o

vgPatchJax: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgPatchJax $O ${MYLIBS} $L
	strip ${BINDIR}/vgPatchJax

clean:
	rm -f $O
