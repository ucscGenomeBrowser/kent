include ../../inc/common.mk


L = -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = dataSim.o

dataSim: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/dataSim $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/dataSim
