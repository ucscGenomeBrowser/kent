include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
BIN=${BINDIR}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = ldHgGene.o
A = ldHgGene

all: ${BIN}/${A}

${BIN}/${A}: $O ${MYLIBS} makefile
	${CC} ${COPT} ${CFLAGS} -o ${BIN}/${A} $O ${MYLIBS} $L

lib:
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O} ${A}
