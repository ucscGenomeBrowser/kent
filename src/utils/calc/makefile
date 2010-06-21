include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = calc.o

calc: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/calc $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/calc${EXE}

clean::
	rm -f ${O}
