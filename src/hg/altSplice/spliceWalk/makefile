
L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a ../lib/$(MACHTYPE)/libSpliceGraph.a

% : %.c
	gcc -Wimplicit -ggdb -I../inc -I../../inc  -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(MYSQLLIBS) -lm
	cp $@ $(HOME)/bin/$(ARCHTYPE)

%Test: %Test.c 
	gcc -Wimplicit -ggdb  -I../inc -I../../inc -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)

O = spliceWalk

TESTO = 

TESTS = 

exe: $(O)


test: $(TESTS)

lib:
	cd ../../lib; make
