include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgProtIdToGenePred.o

hgProtIdToGenePred: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgProtIdToGenePred $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgProtIdToGenePred${EXE}

clean::
	rm -f ${O}
