#OPT= -ggdb 
OPT = -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wuninitialized
#OPT= -ggdb -pg

MYLIBDIR = ../../../lib/$(MACHTYPE)
LIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a ./$(MACHTYPE)/libSpliceGraph.a

#Optimized for performance
.c.o:
	gcc $(OPT) -Wformat -Wimplicit  -I../inc -I../../inc -I../../../inc -I$(MYSQLINC) -c $*.c

%Test: %Test.c 
	gcc -Wimplicit -ggdb  -I../inc -I../../inc -I../../../inc -I$(MYSQLINC)  -o $@ $(addsuffix .c,$@) $(LIBS) 

#For debugging
#.c.o:
#	gcc -ggdb -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wformat -Wimplicit  -I../inc -I../../inc -I$(MYSQLINC) -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = spliceGraph.o exonGraph.o heap.o 

TESTS = heapTest

./$(MACHTYPE)/libSpliceGraph.a: $(O) 
	@[ -d $(MACHTYPE) ] || mkdir -p $(MACHTYPE)	
	ar rcu ./$(MACHTYPE)/libSpliceGraph.a $(O)
	ranlib ./$(MACHTYPE)/libSpliceGraph.a

clean:
	rm -f $(O);
	rm -f $(TESTS)
	rm -f ./$(MACHTYPE)/jkhgap.a;

ctags:
	ctags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tests: 	$(TESTS) $(O)

