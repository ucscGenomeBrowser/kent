include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgLoadJax.o

vgLoadJax: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgLoadJax $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgLoadJax

clean:
	rm -f $O
