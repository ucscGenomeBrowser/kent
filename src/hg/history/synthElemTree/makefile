include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/element.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS=-ggdb

O = synthElemTree.o

synthElemTree: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/synthElemTree $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/synthElemTree${EXE}

