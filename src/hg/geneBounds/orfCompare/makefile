include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = orfCompare.o

orfCompare: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/orfCompare $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/orfCompare${EXE}

test:	orfCompare
	orfCompare inA.orf inB.orf > out.txt
	diff expected.txt out.txt

clean:
	rm -f ${O}
