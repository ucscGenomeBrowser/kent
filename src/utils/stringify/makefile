include ../../inc/common.mk


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = stringify.o

stringify: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/stringify $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/stringify
