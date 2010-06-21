include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = mafRanges
O = ${A}.o

all: ${A}

${A}: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A}${EXE}

test:	${A}
	cd tests && ${MAKE}

clean::
	rm -f ${O}
