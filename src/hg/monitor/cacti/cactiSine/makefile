include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = cactiSine.o

cactiSine: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/cactiSine $O ${MYLIBS} $L
	strip ${BINDIR}/cactiSine

clean:
	rm -f $O
