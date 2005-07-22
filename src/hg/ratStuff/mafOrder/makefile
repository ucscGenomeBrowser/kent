include ../../../inc/common.mk

CFLAGS= -ggdb
L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
CFLAGS=-ggdb

targets: mafOrder

mafOrder: mafOrder.o $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/mafOrder mafOrder.o $(MYLIBS) $L
