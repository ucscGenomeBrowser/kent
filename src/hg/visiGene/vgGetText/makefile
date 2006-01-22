include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgGetText.o visiGene.o

vgGetText: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgGetText $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgGetText

clean:
	rm -f $O
