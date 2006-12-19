include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = autoDtd.o

autoDtd: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/autoDtd $O ${MYLIBS} $L
	strip ${BINDIR}/autoDtd

clean:
	rm -f $O
	rm -f test/*/out.*

test::
	./doTest
