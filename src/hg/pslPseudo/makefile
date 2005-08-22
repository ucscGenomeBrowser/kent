include ../../inc/common.mk

HG_WARN=-Wall 

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
DEBUGOPTIONS = -skipExciseRepeats -minAli=0.98 -nearTop=0.005 
OPTIONS = -minAli=0.98 -nearTop=0.005 -verbose=3 -stripVersion
#MARKDEBUG = /cse/grads/markd/compbio/dbgmalloc/debug-malloc.o
MARKDEBUG =
DB = hg17
MRNA = mrnaHg17.2bit 
DBDIR = /cluster/data/$(DB)
BED = $(DBDIR)/bed
PSEUDO = $(DBDIR)/bed/pseudo
BUILD=gs.17/build34
VALCMD = chr13.sizes ${PSEUDO}/rmsk.bed $(PSEUDO)/chr13mouseSyn.bed $(PSEUDO)/chr13simple.bed $(PSEUDO)/chr13_mrna.psl 
TESTCMD = $(DBDIR)/chrom.sizes ${PSEUDO}/rmsk.bed.gz $(PSEUDO)/mouseNet.txt $(PSEUDO)/dogNet.txt $(BED)/simpleRepeat/simpleRepeat.bed $(PSEUDO)/all_mrna.psl 
#TESTCMD = $(DBDIR)/chrom.sizes mtRmsk mtSyn.bed mtSimple.bed input/test48.psl
GENES = $(PSEUDO)/refGene.tab $(PSEUDO)/mgcGene.tab $(PSEUDO)/sortedKnownGene.tab


O = pslPseudo.o 

pslPseudo: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslPseudo $O $(MARKDEBUG) $(MYLIBS) $L 
	#${STRIP} $(HOME)/bin/$(MACHTYPE)/pslPseudo${EXE}

lib:
	cd ../../lib && ${MAKE}
	cd ../lib && ${MAKE}

test: test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test13 test14 test15 test16 test17 test18 test19 testRp131 test26 test38
%:	inputHg17/%.psl
	${MKDIR} output
	MALLOC_LOG=malloc.log MALLOC_LOG_OPTS=SAD pslPseudo $(OPTIONS) $(DB) inputHg17/$@.psl $(TESTCMD)  output/$@g.psl output/$@p.psl output/$@l.txt output/$@.axt S1.hg17.lst $(MRNA) $(GENES) > $@.log 2> $@.err
	diff expected/$@g.psl output/$@g.psl 
	diff expected/$@p.psl output/$@p.psl | awk '{print $$15":"$$17"-"$$18}'
	diff expected/$@p.psl output/$@p.psl
#	rm output/$@g.psl output/$@p.psl
#test48:	input/test1.psl
#	${MKDIR} output
#	pslPseudo $(OPTIONS) $(DB) input/$@.psl $(TESTCMD)  output/$@g.psl output/$@p.psl output/$@l.txt output/$@.axt S1.lst AF387618.fa $(GENES)  
#	diff expected/$@g.psl output/$@g.psl 
#	diff expected/$@p.psl output/$@p.psl
##	rm output/$@g.psl output/$@p.psl
#jtest59:	input/test59.psl
#j	${MKDIR} output
#j	MALLOC_LOG=malloc.log MALLOC_LOG_OPTS=SAD  pslPseudo $(OPTIONS) $(DB) input/$@.psl $(VALCMD)  output/$@g.psl output/$@p.psl output/$@l.txt output/$@.axt S1.lst mrnaHg16.fa $(GENES) 2> valgrind.out > test59.log
#valgrind -v --leak-resolution=high --leak-check=yes --num-callers=25 --show-reachable=yes pslPseudo $(OPTIONS) $(DB) input/$@.psl $(VALCMD)  output/$@g.psl output/$@p.psl output/$@l.txt output/$@.axt S1.lst mrnaHg16.fa $(GENES) 2> valgrind.out
	diff expected/$@g.psl output/$@g.psl 
	diff expected/$@p.psl output/$@p.psl

clean:
	rm -fr ${O} output
