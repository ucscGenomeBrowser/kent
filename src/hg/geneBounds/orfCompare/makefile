include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = orfCompare.o

orfCompare: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/orfCompare $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/orfCompare${EXE}

test:
	orfCompare inA.orf inB.orf > out.txt
	diff expected.txt out.txt

clean:
	rm -f $O

