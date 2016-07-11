kentSrc = ../../..
include ../../../inc/common.mk


L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneEnds.o

ooCloneEnds: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ooCloneEnds $O $(MYLIBS) $L

clean::
	rm -f ${O}
