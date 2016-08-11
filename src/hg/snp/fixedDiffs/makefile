kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
#CFLAGS=-ggdb

O = fixedDiffs.o

all: fixedDiffs

fixedDiffs: fixedDiffs.o ${MYLIBS}
	${CC} ${COPT} -O -o fixedDiffs fixedDiffs.o ${MYLIBS} $L


clean::
	rm -f ${O}
