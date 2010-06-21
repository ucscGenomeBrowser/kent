include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = endsInLf.o

endsInLf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/endsInLf $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/endsInLf${EXE}

clean::
	rm -f ${O}
