
include ../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

% : %.c
	gcc -Wimplicit -ggdb -I../inc  -I../../inc  -o $@ $(addsuffix .c,$@) $(O) $(MYLIBS) $(MYSQLLIBS) -lm -lz
	cp $@ $(HOME)/bin/$(ARCHTYPE)

%Test: %Test.c 
	gcc -Wimplicit -ggdb  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)

E =  orthoMap orthoEvaluate orthoPickIntron # spliceStats spliceVis

O = orthoEval.o

exe: compile $(E) 

compile: $(O)

test: $(TESTS)

lib:
	cd ../../lib; make
