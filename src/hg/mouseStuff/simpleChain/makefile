include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
BINDIR = $(HOME)/bin/$(MACHTYPE)

O = simpleChain.o

simpleChain: $O $(MYLIBS)
	$(CC) -o simpleChain $O $(MYLIBS) $L
	cp -f simpleChain $(BINDIR)
#	${STRIP} $(BINDIR)/simpleChain${EXE}
