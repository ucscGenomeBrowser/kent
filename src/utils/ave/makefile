include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = ave.o

ave: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/ave $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/ave${EXE}
