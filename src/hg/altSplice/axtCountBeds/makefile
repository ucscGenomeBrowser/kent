
include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a ../lib/$(MACHTYPE)/libSpliceGraph.a 

% : %.c
	gcc -Wimplicit -ggdb -I../inc -I../../inc  -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(MYSQLLIBS) -lm -lz
	cp $@ $(HOME)/bin/$(ARCHTYPE)

O =  axtCountBeds


exe: $(O)

lib:
	cd ../../lib; make
