include ../../../inc/common.mk

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = fixEnsGtf.o

fixEnsGtf: $O 
	gcc -o fixEnsGtf $O $(MYLIBS) $L

