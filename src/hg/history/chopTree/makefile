include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
CFLAGS=-ggdb

O = chopTree.o

chopTree: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/chopTree $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/chopTree${EXE}

