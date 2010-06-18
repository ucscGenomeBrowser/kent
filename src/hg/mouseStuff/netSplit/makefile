include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netSplit.o

netSplit: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/netSplit $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/netSplit${EXE}

clean::
	rm -f ${O}
