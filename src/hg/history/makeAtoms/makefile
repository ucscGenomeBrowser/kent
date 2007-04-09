include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/element.a
# CFLAGS=-ggdb

O = makeAtoms.o

makeAtoms: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/makeAtoms $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/makeAtoms${EXE}

