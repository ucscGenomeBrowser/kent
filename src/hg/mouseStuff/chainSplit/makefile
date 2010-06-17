include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainSplit.o

chainSplit: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chainSplit $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/chainSplit${EXE}

clean::
	rm -f ${O}
