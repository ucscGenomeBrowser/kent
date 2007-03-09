include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txCdsOrtho.o

txCdsOrtho: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txCdsOrtho $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txCdsOrtho${EXE}

test:
	txCdsOrtho in.cds in.maf out.tab
	diff expected.tab out.tab

clean:
	rm -f $O
