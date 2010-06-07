include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = mafSpeciesSubset
O = ${A}.o

${A}: compile
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A} $O ${MYLIBS} ${L}

clean:
	rm -f $O
