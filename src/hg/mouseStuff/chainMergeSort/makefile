include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainMergeSort.o

chainMergeSort: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/chainMergeSort $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/chainMergeSort
