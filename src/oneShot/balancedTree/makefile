include ../../inc/common.mk


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = balancedTree.o

balancedTree: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/balancedTree $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/balancedTree${EXE}
