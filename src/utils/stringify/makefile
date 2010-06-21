include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = stringify.o

stringify: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/stringify $O $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/stringify${EXE}

clean::
	rm -f ${O}
