include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTpf.o

hgTpf: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgTpf $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgTpf
