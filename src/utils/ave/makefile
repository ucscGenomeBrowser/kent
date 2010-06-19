include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = ave.o

ave: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/ave $O $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/ave${EXE}

clean::
	rm -f ${O}
