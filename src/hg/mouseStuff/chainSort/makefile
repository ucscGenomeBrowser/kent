include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainSort.o

chainSort: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chainSort $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/chainSort${EXE}

clean::
	rm -f ${O}
