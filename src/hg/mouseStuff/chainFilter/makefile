include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainFilter.o

chainFilter: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/chainFilter $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/chainFilter${EXE}
