include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
OPTIONS = -sizeMatters -minAli=0.98 -nearTop=0.005 
DB = hg15
DBDIR = /cluster/data/$(DB)
BED = $(DBDIR)/bed
TESTCMD = $(DBDIR)/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk $(BED)/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed $(BED)/simpleRepeat/simpleRepeat.bed $(BED)/mrna/all_mrna.psl 
GENES = $(BED)/refSeq/refGene.tab $(BED)/softberry/genePred.tab

O = pslPseudo.o 

pslPseudo: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslPseudo $O $(MYLIBS) $L
	#strip $(HOME)/bin/$(MACHTYPE)/pslPseudo

lib:
	cd ../../lib; make

test: test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test13 test14 test15 test16 test17 test18 test19 testRp131
%:	input/%.psl
	mkdir -p output
	pslPseudo $(OPTIONS) $(DB) input/$@.psl $(TESTCMD)  output/$@g.psl output/$@p.psl output/$@l.txt /cluster/data/$(DB)/nib $(GENES)  
	diff expected/$@g.psl output/$@g.psl 
	diff expected/$@p.psl output/$@p.psl
#	rm output/$@g.psl output/$@p.psl
