include ../../../inc/common.mk

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = addChrPsl.o

addChrPsl: $O 
	gcc -o addChrPsl $O $(MYLIBS) $L

