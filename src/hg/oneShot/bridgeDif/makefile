include ../../../inc/common.mk

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bridgeDif.o

bridgeDif: $O 
	gcc -o bridgeDif $O $(MYLIBS) $L

