include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/element.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS=-ggdb

O = elTreeClip.o

elTreeClip: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/elTreeClip $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/elTreeClip${EXE}

