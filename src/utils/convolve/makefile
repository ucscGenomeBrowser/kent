include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

all::	convolve

O = convolve.o

A = convolve

$A: $O ${MYLIBS}
	gcc -ggdb -o ${HOME}/bin/${MACHTYPE}/${A} $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/${A}

test:
	cd tests && ./runtest

clean:
	rm -f $O
