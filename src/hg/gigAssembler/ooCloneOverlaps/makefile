include ../../../inc/common.mk


L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneOverlaps.o

ooCloneOverlaps: $O 
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ooCloneOverlaps $O $(MYLIBS) $L

