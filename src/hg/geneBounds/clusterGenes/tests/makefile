kentSrc = ../../../..
include ../../../../inc/common.mk

clusterGenes = ${DESTBINDIR}/clusterGenes

all:

test: refSeqTest refSeqCdsTest trackNamesTest joinTest joinCdsTest \
	overlapUseStrandTest overlapIgnoreStrandsTest

# test files blatRefSeq.gp  ncbiRefSeq.gp
#  chr1:11,689,948-12,078,317 - some basic genes
#    blat NM_138346.1 edited to have shorter CDS, different exon boundries
#    both NM_000302.2 edited so CDS doesn't overlap
#  chr4:71,201,777-71,250,403 - a merged cluster

refSeqTest: mkout
	${clusterGenes} -conflicted -clusterBed=output/$@.bed -flatBed=output/$@.flat.bed output/$@.gl no input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/$@.gl output/$@.gl
	diff -u expected/$@.bed output/$@.bed
	diff -u expected/$@.flat.bed output/$@.flat.bed

refSeqCdsTest: mkout
	${clusterGenes} -cds -clusterBed=output/$@.bed -clusterTxBed=output/$@.tx.bed output/$@.gl no input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/$@.gl output/$@.gl
	diff -u expected/$@.bed output/$@.bed
	diff -u expected/$@.tx.bed output/$@.tx.bed

trackNamesTest: mkout
	${clusterGenes} -conflicted -cds -trackNames output/$@.gl no fred input/blatRefSeq.gp barney input/ncbiRefSeq.gp
	diff -u expected/$@.gl output/$@.gl

joinTest: mkout
	${clusterGenes} -joinContained -clusterBed=output/$@.bed -flatBed=output/$@.flat.bed output/$@.gl no input/blatRefSeq.gp input/ncbiRefSeq.gp input/joinSmall.gp
	diff -u expected/$@.gl output/$@.gl
	diff -u expected/$@.bed output/$@.bed
	diff -u expected/$@.flat.bed output/$@.flat.bed

joinCdsTest: mkout
	${clusterGenes} -joinContained -cds -clusterBed=output/$@.bed -flatBed=output/$@.flat.bed output/joinCdsTest.gl no input/blatRefSeq.gp input/ncbiRefSeq.gp input/joinSmall.gp
	diff -u expected/$@.gl output/$@.gl
	diff -u expected/$@.bed output/$@.bed
	diff -u expected/$@.flat.bed output/$@.flat.bed

# overlaping genes blocks on opposite stands
overlapUseStrandTest: mkout
	${clusterGenes} -trackNames output/$@.gl no genes input/overlapDiffStrands.gp
	diff -u expected/$@.gl output/$@.gl

# overlaping genes blocks on opposite stands, but ignore strand
overlapIgnoreStrandsTest: mkout
	${clusterGenes} -ignoreStrand -trackNames output/$@.gl no genes input/overlapDiffStrands.gp
	diff -u expected/$@.gl output/$@.gl

mkout:
	${MKDIR} output

clean:
	rm -rf output

# run valgrind
valgrindOpts =  -v --skin=memcheck --num-callers=20 --leak-check=yes --leak-resolution=med --show-reachable=yes
valgrind:  mkout
	valgrind ${valgrindOpts} ${clusterGenes} -clusterBed=output/refSeqTest.bed -flatBed=output/refSeqTest.flat.bed output/refSeqTest.gl no input/blatRefSeq.gp input/ncbiRefSeq.gp
