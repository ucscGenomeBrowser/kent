include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgFlyBase.o

hgFlyBase: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgFlyBase $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/hgFlyBase
