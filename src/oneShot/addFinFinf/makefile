include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = addFinFinf.o

addFinFinf: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/addFinFinf $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/addFinFinf
