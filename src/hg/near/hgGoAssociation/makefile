include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgGoAssociation.o

hgGoAssociation: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgGoAssociation $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgGoAssociation

test:
	hgGoAssociation go goaPart input/test1.goa -tab=output -noLoad -limit=500
	diff -x CVS -r expected output
	rm -r output
