include ../../../../inc/common.mk

all:

test: refSeqTest refSeqCdsTest trackNamesTest joinTest joinCdsTest

# test files blatRefSeq.gp  ncbiRefSeq.gp
#  chr1:11,689,948-12,078,317 - some basic genes
#    blat NM_138346.1 edited to have shorter CDS, different exon boundries
#    both NM_000302.2 edited so CDS doesn't overlap
#  chr4:71,201,777-71,250,403 - a merged cluster

refSeqTest: mkout
	clusterGenes -conflicted -clusterBed=output/$@.bed -flatBed=output/$@.flat.bed output/$@.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/$@.gl output/$@.gl
	diff -u expected/$@.bed output/$@.bed
	diff -u expected/$@.flat.bed output/$@.flat.bed

refSeqCdsTest: mkout
	clusterGenes -cds output/$@.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
	diff -u expected/$@.gl output/$@.gl

trackNamesTest: mkout
	clusterGenes -conflicted -cds -trackNames output/$@.gl hg16 fred input/blatRefSeq.gp barney input/ncbiRefSeq.gp
	diff -u expected/$@.gl output/$@.gl

joinTest: mkout
	clusterGenes -joinContained -clusterBed=output/$@.bed -flatBed=output/$@.flat.bed output/$@.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp input/joinSmall.gp
	diff -u expected/$@.gl output/$@.gl
	diff -u expected/$@.bed output/$@.bed
	diff -u expected/$@.flat.bed output/$@.flat.bed

joinCdsTest: mkout
	clusterGenes -joinContained -cds output/joinCdsTest.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp input/joinSmall.gp
	diff -u expected/joinCdsTest.gl output/joinCdsTest.gl

mkout:
	${MKDIR} output

clean:
	rm -rf output

# run valgrind
valgrindOpts =  -v --skin=memcheck --num-callers=20 --leak-check=yes --leak-resolution=med --show-reachable=yes
valgrind:  mkout
	valgrind ${valgrindOpts} clusterGenes -clusterBed=output/refSeqTest.bed -flatBed=output/refSeqTest.flat.bed output/refSeqTest.gl hg16 input/blatRefSeq.gp input/ncbiRefSeq.gp
