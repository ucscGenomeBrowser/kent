include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
INSTALLDIR = ${HOME}/bin/${MACHTYPE}
SCRIPT_DIR = /cluster/data/encode/bin/scripts
CFLAGS = -g

PROG = regionOrtho
O = ${PROG}.o

all: ${PROG}

${PROG}: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${INSTALLDIR}/${PROG} $O ${MYLIBS} $L
#	strip ${INSTALLDIR}/${PROG}

test: ${INSTALLDIR}/${PROG}
	cd tests && ${MAKE} test
