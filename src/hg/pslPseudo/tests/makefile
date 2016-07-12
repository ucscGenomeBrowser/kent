kentSrc = ../../..
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
OPTIONS = -minAli=0.98 -nearTop=0.005 -verbose=5 -stripVersion
NIBLIST = S1.hg18.lst
GENES = $(PSEUDO)/refGene.tab.gz $(PSEUDO)/mgcGene.tab.gz $(PSEUDO)/sortedKnownGene.tab.gz $(PSEUDO)/rhesusNet.txt.gz

test:	NM_017681 est5Mrna100 overlapTest maxAlignTest 

# nothing should be filtered with no arguments
noopTest:
	${MAKE} doPseudo name=$@ inPsl=overlap.psl filtArgs=''

est5Mrna100:
	${MAKE} doPseudo name=$@ inPsl=pseudoEst5MrnaOrf100.psl filtArgs='' 

NM_017681:
	${MAKE} doPseudo name=$@ inPsl=NM_017681.psl filtArgs='' inSizes=chrom.sizes

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
#ifneq (${inSizes},)
#	sizesOpt=-polyASizes=input/${inSizes}
#endif
outDir=output/${name}
sortPsl=sort -k 10,10 -k 12,12n -k 13,13n
doPseudo:
	@${MKDIR} ${outDir}
	${PSLPSEUDO} $(OPTIONS) ${filtArgs} $(DB) input/${inPsl} $(TESTCMD) ${outDir}/${name}g.psl ${outDir}/keep.psl.tmp ${outDir}/${name}.raw.bed /dev/null ${NIBLIST} $(MRNA) $(GENES) >${outDir}/${name}.log 2>&1
	tawk -f filterBed.awk ${outDir}/${name}.raw.bed | sort -k1,1 -k2,3n -k4,4 >${outDir}/${name}.sort.bed
doSave:
	@${MKDIR} ${outDir}
	${PSLPSEUDO} $(OPTIONS) ${filtArgs} $(DB) input/${inPsl} $(TESTCMD) ${outDir}/${name}g.psl ${outDir}/keep.psl.tmp ${outDir}/${name}.raw.bed /dev/null ${NIBLIST} $(MRNA) $(GENES) >${outDir}/${name}.log 2>&1
	tawk -f filterBed.awk ${outDir}/${name}.raw.bed | sort -k1,1 -k2,3n -k4,4 > ${outDir}/${name}.sort.bed
	bedOverlap -noBin $(outDir}/${name}.sort.bed ${outDir}/${name}.bed
	tawk '{print $1,$2,$3,$4,$5,$6}' ${outDir}/${name}.bed | overlapSelect expected/{name}.est5.bed stdin ${outDir}/${name}.est5.bed -inFmt=bed
	@${sortPsl} ${outDir}/keep.psl.tmp > ${outDir}/keep.psl
	@rm -f ${outDir}/keep.psl.tmp
	${DIFF} expected/${name}.bed output/${name}.bed
	${DIFF} expected/${name}.est5.bed output/${name}.est5.bed
	${DIFF} expected/${name}.psl output/${name}.psl

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
