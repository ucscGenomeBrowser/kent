include ../../../inc/makefile.h

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clonePos.o

clonePos: $O 
	gcc -o clonePos $O $(MYLIBS) $L

