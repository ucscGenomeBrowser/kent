include ../../../inc/common.mk

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = agpVsMap.o

agpVsMap: $O 
	gcc -o $(HOME)/bin/$(MACHTYPE)/agpVsMap $O $(MYLIBS) $L

