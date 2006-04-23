include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgLoadJax.o

vgLoadJax: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgLoadJax $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgLoadJax

forwardSlices: forwardSlices.o ${MYLIBS}
	${CC} ${COPT} -o forwardSlices forwardSlices.o ${MYLIBS} $L

clean:
	rm -f $O
	rm -f forwardSlices forwardSlices.o
