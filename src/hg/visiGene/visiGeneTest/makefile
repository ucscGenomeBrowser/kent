include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = visiGeneTest.o

visiGeneTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/visiGeneTest $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/visiGeneTest

clean:
	rm -f $O
