include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgNearTest.o

hgNearTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgNearTest $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/hgNearTest

test:
	hgNearTest hgwbeta.cse.ucsc.edu/cgi-bin/hgNear org=Yeast repeat=1

clean:
	rm -f $O
