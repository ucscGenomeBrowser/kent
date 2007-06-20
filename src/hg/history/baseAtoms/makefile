include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/element.a
CFLAGS=-ggdb

O = baseAtoms.o

baseAtoms: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/baseAtoms $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/baseAtoms${EXE}

