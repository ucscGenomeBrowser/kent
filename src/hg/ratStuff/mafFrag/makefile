include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafFrag.o

mafFrag: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/mafFrag $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/mafFrag

test:
	mafFrag hg16 humorMm3Rn3 chr2 69065807 69073183 - test.maf
