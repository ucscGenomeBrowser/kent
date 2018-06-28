kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
#CFLAGS=-ggdb

O = fixedDiffs.o

all: fixedDiffs

fixedDiffs: fixedDiffs.o ${MYLIBS}
	${CC} ${COPT} -O -o fixedDiffs fixedDiffs.o ${MYLIBS} $L


clean::
	rm -f ${O}
