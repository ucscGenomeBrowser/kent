include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/element.a
CFLAGS=-ggdb

O = atomPaint.o

atomPaint: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/atomPaint $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/atomPaint${EXE}

