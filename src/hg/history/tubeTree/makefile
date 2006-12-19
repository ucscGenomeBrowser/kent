include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/element.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS=-ggdb

O = tubeTree.o

tubeTree: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/tubeTree $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/tubeTree${EXE}

