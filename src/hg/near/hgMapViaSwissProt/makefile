include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgMapViaSwissProt.o

hgMapViaSwissProt: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgMapViaSwissProt $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgMapViaSwissProt${EXE}
