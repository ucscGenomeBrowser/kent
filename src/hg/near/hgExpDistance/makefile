include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgExpDistance.o

hgExpDistance: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgExpDistance $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/hgExpDistance
