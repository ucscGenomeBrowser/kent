include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/element.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS=-ggdb

O = printTree.o

printTree: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/printTree $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/printTree${EXE}

