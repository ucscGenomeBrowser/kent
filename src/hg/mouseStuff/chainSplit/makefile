include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainSplit.o

chainSplit: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/chainSplit $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/chainSplit${EXE}
