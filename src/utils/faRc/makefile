include ../../inc/common.mk


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faRc.o

faRc: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faRc $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/faRc
