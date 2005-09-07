include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgPatchJax.o

vgPatchJax: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgPatchJax $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgPatchJax

clean:
	rm -f $O
