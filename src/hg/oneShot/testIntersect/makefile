kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = testIntersect.o

testIntersect: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/testIntersect $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/testIntersect${EXE}

clean:
	rm -f $O
