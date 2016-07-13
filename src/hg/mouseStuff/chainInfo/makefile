kentSrc = ../../..
include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainInfo.o

chainInfo: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -O -o ${BINDIR}/chainInfo $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/chainInfo${EXE}
test: 22 7 
%:	input/chr%.chain
	chainInfo mm3 chr$@ input/chr$@.chain /cluster/store5/gs.16/build33//bed/blastz.mm3.2003-04-12-03-MS/axtSyn/chr$@.bed /cluster/store5/gs.16/build33//chrom.sizes /cluster/store2/mm.2003.02/mm3/chrom.sizes /cluster/store5/gs.16/build33/$@/chr$@.fa.out /scratch/mm3/rmsk/ output/chr$@.out /cluster/store2/mm.2003.02/mm3/bed/refSeq/refGene.tab /cluster/store2/mm.2003.02/mm3/bed/softberry/genePred.tab /cluster/store5/gs.16/build33//bed/bedOver/vega/new.gp /cluster/store2/mm.2003.02/mm3/bed/mrna/mrna.tab
	diff expected/chr$@.out output/chr$@.out 
	rm output/chr$@.out 

clean::
	rm -f ${O}
