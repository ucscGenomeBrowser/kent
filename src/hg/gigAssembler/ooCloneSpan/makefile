kentSrc = ../../..
include ../../../inc/common.mk


L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneSpan.o

ooCloneSpan: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ooCloneSpan $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/ooCloneSpan${EXE}

clean::
	rm -f ${O}
