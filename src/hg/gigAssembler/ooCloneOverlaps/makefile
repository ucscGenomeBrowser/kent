kentSrc = ../../..
include ../../../inc/common.mk


L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneOverlaps.o

ooCloneOverlaps: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ooCloneOverlaps $O $(MYLIBS) $L

clean::
	rm -f ${O}
