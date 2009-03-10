include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = ave.o

ave: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ave $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/ave${EXE}
