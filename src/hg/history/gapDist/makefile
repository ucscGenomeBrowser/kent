include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/element.a
CFLAGS=-ggdb

O = gapDist.o

gapDist: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gapDist $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/gapDist${EXE}

