kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

O = testCache.o

testCache: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/testCache $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/testCache${EXE}

clean:
	rm -f $O
