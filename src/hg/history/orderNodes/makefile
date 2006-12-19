include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/element.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS=-ggdb

O = orderNodes.o

orderNodes: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/orderNodes $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/orderNodes${EXE}

