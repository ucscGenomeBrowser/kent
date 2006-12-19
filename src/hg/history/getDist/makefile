include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/element.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS=-ggdb

O = getDist.o

getDist: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/getDist $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/getDist${EXE}

