kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = raMerge.o

raMerge: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/raMerge $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/raMerge${EXE}

test:
	raMerge name in1.ra in2.ra in3.ra > out.ra
	diff expected.ra out.ra

clean:
	rm -f $O
