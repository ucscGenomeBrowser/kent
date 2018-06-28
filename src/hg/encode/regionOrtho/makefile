kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
INSTALLDIR = ${BINDIR}
SCRIPT_DIR = /cluster/data/encode/bin/scripts
CFLAGS = -g

PROG = regionOrtho
O = ${PROG}.o

all: ${PROG}

${PROG}: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${INSTALLDIR}/${PROG} $O ${MYLIBS} $L
#	${STRIP} ${INSTALLDIR}/${PROG}${EXE}

test: ${INSTALLDIR}/${PROG}
	cd tests && ${MAKE} test

clean::
	rm -f ${O}
