include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgAccessCrawl.o

hgAccessCrawl: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgAccessCrawl $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgAccessCrawl${EXE}
