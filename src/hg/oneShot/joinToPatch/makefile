include ../../../inc/common.mk

L = 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = joinToPatch.o

joinToPatch: $O 
	gcc -o joinToPatch $O $(MYLIBS) $L

