include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faPolyASizes.o

faPolyASizes: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/faPolyASizes${EXE} $O $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/faPolyASizes${EXE}

clean::
	rm -f ${O}
