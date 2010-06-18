include ../../../inc/common.mk

CFLAGS= -ggdb
L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
CFLAGS=-ggdb

O = mafOrder.o

targets: mafOrder

mafOrder: ${O} $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mafOrder mafOrder.o $(MYLIBS) $L

clean::
	rm -f ${O}
