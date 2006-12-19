include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/element.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS=-ggdb

O = elTreeComp.o

elTreeComp: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/elTreeComp $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/elTreeComp${EXE}

