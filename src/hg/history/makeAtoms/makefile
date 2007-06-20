include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/element.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS=-ggdb

O = makeAtoms.o

makeAtoms: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/makeAtoms $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/makeAtoms${EXE}

