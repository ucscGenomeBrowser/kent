include ../../inc/common.mk

L = -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  ../lib/$(MACHTYPE)/paralib.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = broadNode.o

broadNode: $O $(MYLIBS)
	gcc -ggdb -o ../bin/broadNode $O $(MYLIBS) $L
	strip ../bin/broadNode
