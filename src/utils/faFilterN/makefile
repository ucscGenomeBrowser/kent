include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faFilterN.o

faFilterN: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faFilterN $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/faFilterN
