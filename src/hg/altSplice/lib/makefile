OPT= -ggdb 

#Optimized for performance
.c.o:
	gcc -ggdb -O -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wformat -Wimplicit -Wuninitialized -I../inc -I../../inc -I../../../inc -I$(MYSQLINC) -c $*.c

#For debugging
#.c.o:
#	gcc -ggdb -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wformat -Wimplicit  -I../inc -I../../inc -I$(MYSQLINC) -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = spliceGraph.o

./$(MACHTYPE)/libSpliceGraph.a: $(O) 
	@[ -d $(MACHTYPE) ] || mkdir -p $(MACHTYPE)	
	ar rcu ./$(MACHTYPE)/libSpliceGraph.a $(O)
	ranlib ./$(MACHTYPE)/libSpliceGraph.a

clean:
	rm -f $(O);
	rm -f ./$(MACHTYPE)/jkhgap.a;

ctags:
	ctags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tests: 	
#	gcc -Wimplicit -ggdb   -I../../inc -I../inc  -o test_ggMrnaAli test_ggMrnaAli.c  $(MYLIBS) $L
	gcc -Wimplicit -ggdb   -I../../inc -I../inc  -o test_geneGraph test_geneGraph.c  $(MYLIBS) $L
