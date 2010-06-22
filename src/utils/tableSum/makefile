include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = tableSum.o

tableSum: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/tableSum $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/tableSum${EXE}

clean::
	rm -f ${O}
