kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testPoly.o

testPoly: $O ${MYLIBS}
	${CC} ${COPT} -o testPoly $O ${MYLIBS} $L
	${STRIP} testPoly${EXE}

clean:
	rm -f $O
