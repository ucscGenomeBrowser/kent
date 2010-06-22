include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faFilterN.o

faFilterN: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faFilterN $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/faFilterN${EXE}

clean::
	rm -f ${O}
