include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = venn.o

venn: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/venn $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/venn${EXE}

clean::
	rm -f ${O}
