include ../../../inc/common.mk
#OPT= -O2
#OPT = -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wuninitialized
OPT= -ggdb 
L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a ../lib/$(MACHTYPE)/libSpliceGraph.a 

% : %.c
	${CC} ${COPT} ${CFLAGS} $(OPT) -Wformat -Wimplicit -I../inc -I../../inc  -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) ${L} $(MYSQLLIBS) -lm
	cp $@ ${BINDIR}

%Test: %Test.c 
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) ${L}

O =  exonWalk borfMatcher

TESTO = 

TESTS = 

exe: $(O)

clean: 
	rm -f $(O)

track:
	#./exonWalk ranbp1.agx exonWalk.bed -diagnostics -trim
	time ./exonWalk /cluster/home/sugnet/store1/altSplice/hg10.chr22.rnaCluster.2.tab exonWalk.bed -diagnostics  >& exonWalk.log
	cat cleanEsts.bed  exonSites.bed rejects.bed exonWalk.bed whites.bed > out.bed
	chmod 777 out.bed
	cp out.bed $(HOME)/.html/

test: $(TESTS)

lib:
	cd ../../lib && ${MAKE}
