include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = axtDropOverlap.o 

axtDropOverlap: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/axtDropOverlap $O $(MYLIBS) $L ; cp $(HOME)/bin/$(MACHTYPE)/axtDropOverlap /cluster/home/baertsch/bin

lib:
	cd ../../lib; make
