include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafFrags.o

mafFrags: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/mafFrags $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/mafFrags

test:
	mkdir -p output
	mafFrags hg16 humorMm3Rn3 input/hg16.bed output/hg16.maf -orgs=input/hg16.org
	diff -x CVS -r expected output
	rm -r output

