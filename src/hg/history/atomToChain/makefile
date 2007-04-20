include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/element.a
CFLAGS=-ggdb

O = atomToChain.o

atomToChain: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/atomToChain $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/atomToChain${EXE}

