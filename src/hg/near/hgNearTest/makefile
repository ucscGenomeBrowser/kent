include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgNearTest.o

hgNearTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgNearTest $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/hgNearTest

clean:
	rm -f $O
