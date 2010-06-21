include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = chainMergeSort.o

chainMergeSort: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/chainMergeSort $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/chainMergeSort${EXE}

clean::
	rm -f ${O}
