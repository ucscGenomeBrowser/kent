
L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ../lib/$(MACHTYPE)/libSpliceGraph.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

% : %.c
	gcc -Wimplicit -ggdb -I../inc -I../../inc  -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(MYSQLLIBS) -lm 
	cp $@ $(HOME)/bin/$(ARCHTYPE)

%Test: %Test.c 
	gcc -Wimplicit -ggdb -I../inc -I../../inc -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)

O =  altSplice spliceStats correlateWithExpression

TESTO = test_geneGraph.o

TESTS = altGraphXMemTest geneGraphTest 

exe: $(O)


test: $(TESTS)

clean: 
	rm -rf $(O)
lib:
	cd ../../lib; make
