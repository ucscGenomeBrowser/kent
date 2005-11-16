include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = chainMergeSort.o

chainMergeSort: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/chainMergeSort $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/chainMergeSort${EXE}
