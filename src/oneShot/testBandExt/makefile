include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = testBandExt.o

testBandExt: $O ${MYLIBS}
	${CC} ${COPT} -o testBandExt $O ${MYLIBS} $L
	strip testBandExt

clean:
	rm -f $O
