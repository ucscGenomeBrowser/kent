include ../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = callGraph.o

callGraph: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/callGraph $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/callGraph${EXE}
