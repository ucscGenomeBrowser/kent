include ../../common.mk

PSLPSEUDO = /cluster/home/baertsch/bin/x86_64/pslPseudo 
DIFF = diff -u --exclude CVS -r
DB = hg18
DBDIR = /cluster/data/$(DB)
BED = $(DBDIR)/bed
PSEUDO = $(DBDIR)/bed/pseudo
MRNA = $(PSEUDO)/mrnaNoversion.2bit 
BUILD=gs.19/build36
TESTCMD = $(DBDIR)/chrom.sizes ${PSEUDO}/rmsk.bed.gz $(PSEUDO)/mouseNet.txt.gz $(PSEUDO)/dogNet.txt.gz $(BED)/simpleRepeat/simpleRepeat.bed $(PSEUDO)/all_mrnaFiltered.psl.gz 
OPTIONS = -minAli=0.98 -nearTop=0.005 -verbose=2 -stripVersion
NIBLIST = S1.hg18.lst
GENES = $(PSEUDO)/refGene.tab.gz $(PSEUDO)/mgcGene.tab.gz $(PSEUDO)/sortedKnownGene.tab.gz $(PSEUDO)/rhesusNet.txt.gz



test:	est5Mrna100 overlapTest maxAlignTest 

# nothing should be filtered with no arguments
est5Mrna100:
	${MAKE} doPseudo name=$@ inPsl=pseudoEst5MrnaOrf100.psl filtArgs=''

overlapTest:
	${MAKE} doPseudo name=$@ inPsl=overlap.psl inSizes=overlap.sizes \
	    filtArgs='-bestOverlap -minCover=0.15 -minId=0.96'

maxAlignTest:
	${MAKE} doPseudo name=$@ inPsl=NM_004038.3.psl inSizes=NM_004038.3.sizes \
	    filtArgs='-maxAligns=2'

# Recursive targets.  Results are sorted to allow consistent comparisons, as
# sometimes internal sorting done by pslPseudo is not stable.
#  o name - test name
#  o filtArgs - filter arguments
#  o inPsl - input psl, relative to input dir
#  o inSize - polyA sizes file, relative to input dir (optional)
#$DB $CACHE/split/tmp$1.psl $ICACHE/chrom.sizes $ICACHE/rmsk.bed.gz $ICACHE/mouseNet.txt.gz $ICACHE/dogNet.txt.gz $ICACHE/simpleRepeat.bed.gz $ICACHE/all_mrna.psl.gz $OUT/mrna$1.psl $OUT/pseudo$1.psl /tmp/pseudoMrnaLink$1.txt /tmp/pseudo$1.axt $ICACHE/S1.lst $CACHE/mrna.2bit $ICACHE/refGene.tab.gz $ICACHE/mgcGene.tab.gz $ICACHE/sortedKnownGene.tab.gz $ICACHE/rheMac2Net.txt.gz > /tmp/pseudo$1.log 2> /tmp/err$1.log
ifneq (${inSizes},)
	sizesOpt=-polyASizes=input/${inSizes}
endif
outDir=output/${name}
sortPsl=sort -k 10,10 -k 12,12n -k 13,13n
doPseudo:
	@${MKDIR} ${outDir}
	${PSLPSEUDO} $(OPTIONS) ${filtArgs} $(DB) input/${inPsl} $(TESTCMD) ${outDir}/${name}g.psl ${outDir}/keep.psl.tmp ${outDir}/${name}l.txt /dev/null ${NIBLIST} $(MRNA) $(GENES) >${outDir}/${name}.log 2>&1
	@${sortPsl} ${outDir}/keep.psl.tmp > ${outDir}/keep.psl
	@rm -f ${outDir}/keep.psl.tmp
	${DIFF} expected/${name} output/${name}

# different parameters
doPseudoAlt:
	@${MKDIR} ${outDir}
	${PSLPSEUDO} ${filtArgs} -verbose=1 -dropped=${outDir}/drop.psl.tmp -weirdOverlapped=${outDir}/weird.psl.tmp input/${inPsl} ${outDir}/keep.psl.tmp >${outDir}/filt.out 2>&1
	@${sortPsl} ${outDir}/keep.psl.tmp > ${outDir}/keep.psl
	@${sortPsl} ${outDir}/drop.psl.tmp > ${outDir}/drop.psl 
	@${sortPsl} ${outDir}/weird.psl.tmp > ${outDir}/weird.psl
	@rm -f ${outDir}/keep.psl.tmp ${outDir}/drop.psl.tmp ${outDir}/weird.psl.tmp
	${DIFF} expected/${name} output/${name}
clean:
	rm -rf output
