include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainInfo.o

chainInfo: $O $(MYLIBS)
	gcc -ggdb -O -o $(HOME)/bin/$(MACHTYPE)/chainInfo $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/chainInfo
test: test1 
%:	input/%.chain
	chainInfo mm3 chr7 input/$@.chain /cluster/store5/gs.16/build33//bed/blastz.mm3.2003-04-12-03-MS/axtSyn/chr7.bed /cluster/store5/gs.16/build33//chrom.sizes /cluster/store2/mm.2003.02/mm3/chrom.sizes /cluster/store5/gs.16/build33/7/chr7.fa.out /scratch/mm3/rmsk/ output/$@.out /cluster/store2/mm.2003.02/mm3/bed/refSeq/refGene.tab /cluster/store2/mm.2003.02/mm3/bed/softberry/genePred.tab /cluster/store5/gs.16/build33//bed/bedOver/vega/new.gp /cluster/store2/mm.2003.02/mm3/bed/mrna/mrna.tab
	diff expected/$@.out output/$@.out 
	rm output/$@.out 

