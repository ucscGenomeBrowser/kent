
L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a ../lib/$(MACHTYPE)/libSpliceGraph.a

% : %.c
	gcc -Wimplicit -O3 -I../inc -I../../inc  -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(MYSQLLIBS) -lm
	cp $@ $(HOME)/bin/$(ARCHTYPE)

%Test: %Test.c 
	gcc -Wimplicit -ggdb  -I../inc -I../../inc -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)

O =  exonWalk

TESTO = 

TESTS = 

exe: $(O)

track:
	./exonWalk epi64.agx exonWalk.bed -diagnostics
	cat cleanEsts.bed  exonSites.bed rejects.bed exonWalk.bed > out.bed
	chmod 777 out.bed
	cp out.bed $(HOME)/.html/

test: $(TESTS)

lib:
	cd ../../lib; make
