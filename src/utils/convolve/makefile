include ../../inc/common.mk

L += -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

all::	convolve

O = convolve.o

A = convolve

$A: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A}${EXE}

test:
	cd tests && ./runtest

clean:
	rm -f $O
