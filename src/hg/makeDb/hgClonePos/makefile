include ../../../inc/makefile.h

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgClonePos.o gsSeqInfo.o

hgClonePos: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgClonePos $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
