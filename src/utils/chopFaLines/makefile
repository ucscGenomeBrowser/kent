include ../../inc/common.mk


L =
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = chopFaLines.o

chopFaLines: $O 
	gcc -o $(HOME)/bin/$(MACHTYPE)/chopFaLines $O $(MYLIBS) $L

