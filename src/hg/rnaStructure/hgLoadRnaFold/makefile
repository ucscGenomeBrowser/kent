include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadRnaFold.o

hgLoadRnaFold: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgLoadRnaFold $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgLoadRnaFold

test:
	hgLoadRnaFold -tab=output -noLoad hg16 test input
	diff -x CVS -r expected output
	rm -r output
