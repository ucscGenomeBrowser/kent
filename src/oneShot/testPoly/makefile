include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testPoly.o

testPoly: $O ${MYLIBS}
	${CC} ${COPT} -o testPoly $O ${MYLIBS} $L
	strip testPoly

clean:
	rm -f $O
