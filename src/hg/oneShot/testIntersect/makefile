include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = testIntersect.o

testIntersect: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/testIntersect $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/testIntersect

clean:
	rm -f $O
