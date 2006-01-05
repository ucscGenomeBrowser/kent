include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/element.a
CFLAGS=-ggdb

O = elTreeBuild.o

elTreeBuild: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/elTreeBuild $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/elTreeBuild${EXE}

test1:
	elTreeBuild test/s1.tree test/g1.tree test1.out
