include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = simpleChain.o

simpleChain: $O $(MYLIBS)
	$(CC) -o simpleChain $O $(MYLIBS) $L
	cp -f simpleChain $(BINDIR)
#	${STRIP} $(BINDIR)/simpleChain${EXE}

clean::
	rm -f simpleChain ${O}
