include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
CFLAGS=-ggdb

O = mafAddIRows.o

mafAddIRows: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mafAddIRows $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/mafAddIRows${EXE}

clean::
	rm -f ${O}
