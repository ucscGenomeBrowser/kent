include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgNearTest.o

hgNearTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgNearTest $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/hgNearTest${EXE}

test:
	hgNearTest hgwdev-${USER}.cse.ucsc.edu/cgi-bin/hgNear org=Yeast repeat=1 test.log

clean:
	rm -f $O
