include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgsqldump

O = hgsqldump.o

hgsqldump: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/$A $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/$A

clean:
	rm -f $O
